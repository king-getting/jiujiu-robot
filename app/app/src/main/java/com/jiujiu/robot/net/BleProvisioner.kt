package com.jiujiu.robot.net

import android.annotation.SuppressLint
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.ParcelUuid
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import java.util.UUID
import kotlin.coroutines.resume

/**
 * BLE WiFi 配网：啾啾未联网时，通过蓝牙把 WiFi 名称+密码发给 ESP32。
 *
 * 协议（与固件端约定，APP 硬编码）：
 *   写入特征值 JSON: {ver:1, cmd:"wifi", ssid:"...", pwd:"..."}
 *   ESP32 回写 notify: {ok:true, ip:"192.168.x.x"} → APP 拿到 IP 后切换 HTTP 通道
 *
 * 调用方需先确保定位开关已打开并已授予 BLUETOOTH_SCAN / BLUETOOTH_CONNECT 权限。
 */
@SuppressLint("MissingPermission")
class BleProvisioner(private val context: Context) {

    companion object {
        // 自定义 UUID（固件端需使用同一组值，见 app/README.md）
        val SERVICE_UUID: UUID = UUID.fromString("6a696f00-0000-1000-8000-00805f9b34fb")
        val CHAR_WRITE_UUID: UUID = UUID.fromString("6a696f01-0000-1000-8000-00805f9b34fb")
        val CHAR_NOTIFY_UUID: UUID = UUID.fromString("6a696f02-0000-1000-8000-00805f9b34fb")
        val CCC_DESCRIPTOR_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        const val DEVICE_NAME = "JIUJIU"
    }

    sealed class Result {
        /** 配网成功，data 为 ESP32 回传的局域网 IP */
        data class Success(val ip: String) : Result()
        data class Failure(val reason: String) : Result()
    }

    private val adapter by lazy {
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    }

    suspend fun provision(ssid: String, pwd: String): Result = try {
        withTimeout(Protocol.BLE_TIMEOUT_MS) { doProvision(ssid, pwd) }
    } catch (e: Exception) {
        Result.Failure("配网超时或失败：${e.message ?: "未知原因"}")
    }

    private suspend fun doProvision(ssid: String, pwd: String): Result =
        suspendCancellableCoroutine { cont ->
            var gatt: BluetoothGatt? = null
            var resumed = false

            fun finish(result: Result) {
                if (resumed) return
                resumed = true
                gatt?.disconnect()
                gatt?.close()
                adapter?.bluetoothLeScanner?.stopScan(scanCallback)
                cont.resume(result)
            }

            val gattCallback = object : BluetoothGattCallback() {

                override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
                    when {
                        newState == BluetoothProfile.STATE_CONNECTED -> g.discoverServices()
                        newState == BluetoothProfile.STATE_DISCONNECTED ->
                            finish(Result.Failure("蓝牙连接断开（status=$status）"))
                    }
                }

                override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                    val service = g.getService(SERVICE_UUID)
                    val notifyChar = service?.getCharacteristic(CHAR_NOTIFY_UUID)
                    if (service == null || notifyChar == null) {
                        finish(Result.Failure("设备上未找到配网服务，固件 UUID 是否一致？"))
                        return
                    }
                    gatt = g
                    // 先开 notify，再写 WiFi 凭证
                    g.setCharacteristicNotification(notifyChar, true)
                    val descriptor = notifyChar.getDescriptor(CCC_DESCRIPTOR_UUID)
                    if (Build.VERSION.SDK_INT >= 33) {
                        g.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                    } else {
                        @Suppress("DEPRECATION")
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        @Suppress("DEPRECATION")
                        g.writeDescriptor(descriptor)
                    }
                }

                override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
                    val writeChar = g.getService(SERVICE_UUID)?.getCharacteristic(CHAR_WRITE_UUID)
                    if (writeChar == null) {
                        finish(Result.Failure("设备上未找到写入特征值"))
                        return
                    }
                    val payload = JSONObject().apply {
                        put("ver", Protocol.VER)
                        put("cmd", Protocol.CMD_WIFI)
                        put("ssid", ssid)
                        put("pwd", pwd)
                    }.toString().toByteArray(Charsets.UTF_8)

                    if (Build.VERSION.SDK_INT >= 33) {
                        g.writeCharacteristic(
                            writeChar, payload,
                            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                        )
                    } else {
                        @Suppress("DEPRECATION")
                        writeChar.value = payload
                        @Suppress("DEPRECATION")
                        g.writeCharacteristic(writeChar)
                    }
                }

                @Deprecated("API < 33")
                override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
                    @Suppress("DEPRECATION")
                    handleNotify(c.value)
                }

                override fun onCharacteristicChanged(
                    g: BluetoothGatt,
                    c: BluetoothGattCharacteristic,
                    value: ByteArray
                ) {
                    handleNotify(value)
                }

                private fun handleNotify(value: ByteArray?) {
                    val json = runCatching {
                        JSONObject(String(value ?: return, Charsets.UTF_8))
                    }.getOrElse {
                        finish(Result.Failure("设备回执不是合法 JSON")); return
                    }
                    if (json.optBoolean("ok")) {
                        val ip = json.optString("ip")
                        if (ip.isNotBlank()) finish(Result.Success(ip))
                        else finish(Result.Failure("设备回执缺少 ip 字段"))
                    } else {
                        finish(Result.Failure(json.optString("err", "设备配网失败")))
                    }
                }
            }

            val scanCallback = object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) {
                    adapter?.bluetoothLeScanner?.stopScan(this)
                    result.device.connectGatt(context, false, gattCallback)
                }

                override fun onScanFailed(errorCode: Int) {
                    finish(Result.Failure("扫描不到啾啾（错误码 $errorCode），请确认它处于配网模式"))
                }
            }

            cont.invokeOnCancellation {
                gatt?.disconnect(); gatt?.close()
                adapter?.bluetoothLeScanner?.stopScan(scanCallback)
            }

            val filter = ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(SERVICE_UUID))
                .build()
            val settings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build()
            adapter?.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)
                ?: finish(Result.Failure("本机蓝牙不可用"))
        }
}

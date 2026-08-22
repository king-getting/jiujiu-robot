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
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import java.util.UUID
import kotlin.coroutines.resume

/**
 * BLE 直连发消息：扫描 JIUJIU → 连接 → 写入 JSON 消息 → 等 notify 回执。
 * 连接成功后复用；断开后下次发送自动重连。UUID 与固件端一致（见 app/README.md）。
 */
@SuppressLint("MissingPermission")
class BleMessenger(private val appContext: Context) {

    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("6a696f00-0000-1000-8000-00805f9b34fb")
        val CHAR_WRITE_UUID: UUID = UUID.fromString("6a696f01-0000-1000-8000-00805f9b34fb")
        val CHAR_NOTIFY_UUID: UUID = UUID.fromString("6a696f02-0000-1000-8000-00805f9b34fb")
        val CCC_DESCRIPTOR_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        const val DEVICE_NAME = "JIUJIU"
        private const val SEND_TIMEOUT_MS = 12000L
    }

    sealed class Result {
        data class Success(val ack: String = "") : Result()
        data class Failure(val reason: String) : Result()
    }

    private val adapter by lazy {
        (appContext.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    }

    private var gatt: BluetoothGatt? = null
    private var writeChar: BluetoothGattCharacteristic? = null
    private var notifyChar: BluetoothGattCharacteristic? = null
    private var connected = false
    private var scanCallback: ScanCallback? = null

    /** 挂起的协程：连接完成回调 */
    private var connectWaiter: kotlin.coroutines.Continuation<Result>? = null
    /** 挂起的协程：等待 notify 回执 */
    private var ackWaiter: kotlin.coroutines.Continuation<Result>? = null

    suspend fun sendMessage(text: String): Result = try {
        withTimeout(SEND_TIMEOUT_MS) {
            if (!connected) {
                when (val r = doConnect()) {
                    is Result.Failure -> return@withTimeout r
                    else -> {}
                }
            }
            doSend(text)
        }
    } catch (e: TimeoutCancellationException) {
        Result.Failure("蓝牙响应超时，请确认啾啾已开机且在附近")
    } catch (e: Exception) {
        Result.Failure("蓝牙发送失败：${e.message ?: "未知原因"}")
    }

    fun disconnect() {
        scanCallback?.let { runCatching { adapter?.bluetoothLeScanner?.stopScan(it) } }
        scanCallback = null
        runCatching { gatt?.disconnect() }
        runCatching { gatt?.close() }
        gatt = null
        writeChar = null
        notifyChar = null
        connected = false
    }

    // ---------- 内部实现 ----------

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when {
                newState == BluetoothProfile.STATE_CONNECTED -> g.discoverServices()
                newState == BluetoothProfile.STATE_DISCONNECTED -> {
                    connected = false
                    ackWaiter?.resume(Result.Failure("蓝牙连接已断开"))
                    ackWaiter = null
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                failConnect("服务发现失败(status=$status)")
                return
            }
            val service = g.getService(SERVICE_UUID)
            if (service == null) {
                failConnect("设备上未找到服务，固件 UUID 是否一致？")
                return
            }
            writeChar = service.getCharacteristic(CHAR_WRITE_UUID)
            notifyChar = service.getCharacteristic(CHAR_NOTIFY_UUID)
            if (writeChar == null || notifyChar == null) {
                failConnect("设备上未找到特征值")
                return
            }
            g.setCharacteristicNotification(notifyChar, true)
            val desc = notifyChar!!.getDescriptor(CCC_DESCRIPTOR_UUID)
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeDescriptor(desc, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION")
                desc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                g.writeDescriptor(desc)
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            connected = status == BluetoothGatt.GATT_SUCCESS
            if (connected) {
                connectWaiter?.resume(Result.Success())
            } else {
                connectWaiter?.resume(Result.Failure("订阅通知失败(status=$status)"))
            }
            connectWaiter = null
        }

        @Deprecated("API < 33")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION")
            handleAck(c.value)
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            c: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleAck(value)
        }
    }

    private suspend fun doConnect(): Result = suspendCancellableCoroutine { cont ->
        connectWaiter = cont
        cont.invokeOnCancellation { cleanupAfterCancel() }
        val callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                adapter?.bluetoothLeScanner?.stopScan(this)
                gatt = result.device.connectGatt(appContext, false, gattCallback)
            }

            override fun onScanFailed(errorCode: Int) {
                if (connectWaiter === cont) {
                    connectWaiter = null
                    cont.resume(Result.Failure("扫描不到啾啾（错误码 $errorCode），请确认它已开机"))
                }
            }
        }
        scanCallback = callback
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        if (adapter?.bluetoothLeScanner?.startScan(listOf(filter), settings, callback) == null) {
            connectWaiter = null
            cont.resume(Result.Failure("本机蓝牙不可用，请先打开蓝牙"))
        }
    }

    private suspend fun doSend(text: String): Result = suspendCancellableCoroutine { cont ->
        ackWaiter = cont
        val wc = writeChar
        if (wc == null) {
            ackWaiter = null
            cont.resume(Result.Failure("尚未连接，请重试"))
            return@suspendCancellableCoroutine
        }
        val payload = JSONObject().apply {
            put("ver", Protocol.VER)
            put("cmd", Protocol.CMD_MSG)
            put("text", text)
            put("tts", false)
        }.toString().toByteArray(Charsets.UTF_8)
        if (Build.VERSION.SDK_INT >= 33) {
            gatt?.writeCharacteristic(wc, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION")
            wc.value = payload
            @Suppress("DEPRECATION")
            gatt?.writeCharacteristic(wc)
        }
    }

    private fun handleAck(value: ByteArray?) {
        val json = runCatching {
            JSONObject(String(value ?: return, Charsets.UTF_8))
        }.getOrElse { return }
        val waiter = ackWaiter ?: return
        ackWaiter = null
        if (json.optBoolean("ok")) {
            waiter.resume(Result.Success(json.optString("ip")))
        } else {
            waiter.resume(Result.Failure(json.optString("err", "设备拒绝")))
        }
    }

    private fun failConnect(reason: String) {
        connectWaiter?.resume(Result.Failure(reason))
        connectWaiter = null
    }

    private fun cleanupAfterCancel() {
        scanCallback?.let { runCatching { adapter?.bluetoothLeScanner?.stopScan(it) } }
        scanCallback = null
    }
}
package com.jiujiu.robot.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.jiujiu.robot.AppConfig.api
import com.jiujiu.robot.AppConfig.deviceIp
import com.jiujiu.robot.AppConfig.mockMode
import com.jiujiu.robot.databinding.FragmentSettingsBinding
import com.jiujiu.robot.net.ApiResult
import com.jiujiu.robot.net.BleProvisioner
import kotlinx.coroutines.launch

/** 设置页：Mock 开关、设备 IP、连接测试、BLE WiFi 配网（功能 3） */
class SettingsFragment : Fragment() {

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    private val blePermissions: Array<String>
        get() = if (Build.VERSION.SDK_INT >= 31) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { grants ->
            if (grants.all { it.value }) startProvision()
            else binding.textProvisionResult.text = "需要蓝牙权限才能配网"
        }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSettingsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val context = requireContext()

        binding.switchMock.isChecked = context.mockMode
        binding.editIp.setText(context.deviceIp)

        binding.switchMock.setOnCheckedChangeListener { _, checked ->
            context.mockMode = checked
        }
        binding.editIp.setOnFocusChangeListener { _, hasFocus ->
            if (!hasFocus) context.deviceIp = binding.editIp.text?.toString()?.trim().orEmpty()
        }

        binding.btnCheckStatus.setOnClickListener {
            context.deviceIp = binding.editIp.text?.toString()?.trim().orEmpty()
            checkStatus()
        }
        binding.btnProvision.setOnClickListener { ensurePermissionsThenProvision() }
    }

    private fun checkStatus() {
        binding.textDeviceStatus.text = "连接中…"
        viewLifecycleOwner.lifecycleScope.launch {
            when (val result = requireContext().api().status()) {
                is ApiResult.Ok -> binding.textDeviceStatus.text =
                    "在线 ✅  IP=${result.data.ip}  WiFi=${if (result.data.wifi) "✓" else "✗"}" +
                        "  SD=${if (result.data.sd) "✓" else "✗"}  电量=${result.data.battery}%"
                is ApiResult.Err -> binding.textDeviceStatus.text = "连不上：${result.message}"
            }
        }
    }

    private fun ensurePermissionsThenProvision() {
        val missing = blePermissions.filter {
            ContextCompat.checkSelfPermission(requireContext(), it) !=
                PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) startProvision()
        else permissionLauncher.launch(missing.toTypedArray())
    }

    private fun startProvision() {
        val ssid = binding.editSsid.text?.toString()?.trim().orEmpty()
        val pwd = binding.editPwd.text?.toString().orEmpty()
        if (ssid.isEmpty()) {
            binding.textProvisionResult.text = "先填 WiFi 名称"
            return
        }
        binding.btnProvision.isEnabled = false
        binding.textProvisionResult.text = "正在搜索啾啾…请确认它处于配网模式"
        viewLifecycleOwner.lifecycleScope.launch {
            when (val result = BleProvisioner(requireContext()).provision(ssid, pwd)) {
                is BleProvisioner.Result.Success -> {
                    val context = requireContext()
                    context.deviceIp = result.ip
                    context.mockMode = false
                    binding.editIp.setText(result.ip)
                    binding.switchMock.isChecked = false
                    binding.textProvisionResult.text =
                        "配网成功 ✅ 啾啾已上线：${result.ip}（已切换到真机模式）"
                }
                is BleProvisioner.Result.Failure ->
                    binding.textProvisionResult.text = "配网失败：${result.reason}"
            }
            binding.btnProvision.isEnabled = true
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}

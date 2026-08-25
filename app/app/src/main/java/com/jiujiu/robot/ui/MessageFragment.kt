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
import com.jiujiu.robot.databinding.FragmentMessageBinding
import com.jiujiu.robot.net.ApiResult
import com.jiujiu.robot.net.BleMessenger
import kotlinx.coroutines.launch

/** 功能 1：发消息到屏幕。支持 WiFi(HTTP) 与 蓝牙直连(BLE) 两种通道。 */
class MessageFragment : Fragment() {

    private var _binding: FragmentMessageBinding? = null
    private val binding get() = _binding!!

    private val bleMessenger by lazy { BleMessenger(requireContext().applicationContext) }

    private var pendingText: String? = null
    private var pendingChat = false

    private val blePermissions: Array<String>
        get() = if (Build.VERSION.SDK_INT >= 31) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { grants ->
            if (grants.all { it.value }) {
                pendingText?.let { sendViaBle(it, pendingChat) }
            } else {
                binding.textResult.text = "需要蓝牙权限才能直连啾啾"
            }
            pendingText = null
            pendingChat = false
        }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentMessageBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.btnSend.setOnClickListener {
            val text = binding.editMessage.text?.toString()?.trim().orEmpty()
            if (text.isEmpty()) {
                binding.textResult.text = "先写点什么吧"
                return@setOnClickListener
            }
            if (binding.switchChat.isChecked) {
                if (binding.switchBle.isChecked) ensurePermissionsThenBle(text, chat = true)
                else sendViaHttp(text, chat = true)
            } else {
                if (binding.switchBle.isChecked) ensurePermissionsThenBle(text)
                else sendViaHttp(text)
            }
        }
    }

    private fun ensurePermissionsThenBle(text: String, chat: Boolean = false) {
        val missing = blePermissions.filter {
            ContextCompat.checkSelfPermission(requireContext(), it) !=
                PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) sendViaBle(text, chat)
        else {
            pendingText = text
            pendingChat = chat
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun sendViaHttp(text: String, chat: Boolean = false) {
        setSending()
        viewLifecycleOwner.lifecycleScope.launch {
            // tts 恒为 false：语音输出已砍掉，字段保留兼容协议
            when (val result = if (chat) requireContext().api().sendChat(text) else requireContext().api().sendMessage(text, tts = false)) {
                is ApiResult.Ok -> {
                    binding.textResult.text = "已送达 ✅ 啾啾收到啦"
                    binding.editMessage.text?.clear()
                }
                is ApiResult.Err -> binding.textResult.text = "发送失败：${result.message}"
            }
            setReady()
        }
    }

    private fun sendViaBle(text: String, chat: Boolean = false) {
        setSending()
        viewLifecycleOwner.lifecycleScope.launch {
            when (val result = if (chat) bleMessenger.sendChat(text) else bleMessenger.sendMessage(text)) {
                is BleMessenger.Result.Success -> {
                    binding.textResult.text = "已送达 ✅ 啾啾收到啦（蓝牙）"
                    binding.editMessage.text?.clear()
                }
                is BleMessenger.Result.Failure -> binding.textResult.text = "发送失败：${result.reason}"
            }
            setReady()
        }
    }

    private fun setSending() {
        binding.btnSend.isEnabled = false
        binding.textResult.text = "发送中…"
    }

    private fun setReady() {
        binding.btnSend.isEnabled = true
    }

    override fun onDestroyView() {
        super.onDestroyView()
        bleMessenger.disconnect()
        _binding = null
    }
}

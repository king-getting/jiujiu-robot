package com.jiujiu.robot.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.jiujiu.robot.AppConfig.api
import com.jiujiu.robot.databinding.FragmentMessageBinding
import com.jiujiu.robot.net.ApiResult
import kotlinx.coroutines.launch

/** 功能 1：远程发消息到屏幕（tts 为协议预留字段，语音模块未装） */
class MessageFragment : Fragment() {

    private var _binding: FragmentMessageBinding? = null
    private val binding get() = _binding!!

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
            send(text)
        }
    }

    private fun send(text: String) {
        binding.btnSend.isEnabled = false
        binding.textResult.text = "发送中…"
        viewLifecycleOwner.lifecycleScope.launch {
            // tts 恒为 false：语音输出已砍掉，字段保留兼容协议
            when (val result = requireContext().api().sendMessage(text, tts = false)) {
                is ApiResult.Ok -> {
                    binding.textResult.text = "已送达 ✅ 啾啾收到啦"
                    binding.editMessage.text?.clear()
                }
                is ApiResult.Err -> binding.textResult.text = "发送失败：${result.message}"
            }
            binding.btnSend.isEnabled = true
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}

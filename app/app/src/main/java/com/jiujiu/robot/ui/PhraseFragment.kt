package com.jiujiu.robot.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.RecyclerView
import com.jiujiu.robot.AppConfig.api
import com.jiujiu.robot.databinding.FragmentPhraseBinding
import com.jiujiu.robot.databinding.ItemPhraseBinding
import com.jiujiu.robot.net.ApiResult
import kotlinx.coroutines.launch

/** 功能 4：鼓励语管理（查看/添加/按序号删除） */
class PhraseFragment : Fragment() {

    private var _binding: FragmentPhraseBinding? = null
    private val binding get() = _binding!!
    private val adapter = PhraseAdapter { index -> delete(index) }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentPhraseBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.recyclerPhrases.adapter = adapter
        binding.btnAdd.setOnClickListener {
            val text = binding.editPhrase.text?.toString()?.trim().orEmpty()
            if (text.isEmpty()) {
                binding.textStatus.text = "先写一句鼓励语吧"
                return@setOnClickListener
            }
            add(text)
        }
        reload()
    }

    private fun reload() {
        viewLifecycleOwner.lifecycleScope.launch {
            when (val result = requireContext().api().phraseList()) {
                is ApiResult.Ok -> {
                    adapter.submit(result.data)
                    binding.textStatus.text = "共 ${result.data.size} 条"
                }
                is ApiResult.Err -> binding.textStatus.text = "加载失败：${result.message}"
            }
        }
    }

    private fun add(text: String) {
        binding.btnAdd.isEnabled = false
        viewLifecycleOwner.lifecycleScope.launch {
            when (val result = requireContext().api().phraseAdd(text)) {
                is ApiResult.Ok -> {
                    binding.editPhrase.text?.clear()
                    binding.textStatus.text = "已添加 ✅"
                    reload()
                }
                is ApiResult.Err -> binding.textStatus.text = "添加失败：${result.message}"
            }
            binding.btnAdd.isEnabled = true
        }
    }

    private fun delete(index: Int) {
        viewLifecycleOwner.lifecycleScope.launch {
            when (val result = requireContext().api().phraseDel(index)) {
                is ApiResult.Ok -> {
                    binding.textStatus.text = "已删除"
                    reload()
                }
                is ApiResult.Err -> binding.textStatus.text = "删除失败：${result.message}"
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}

private class PhraseAdapter(
    private val onDelete: (Int) -> Unit
) : RecyclerView.Adapter<PhraseAdapter.VH>() {

    private val items = mutableListOf<String>()

    fun submit(list: List<String>) {
        items.clear()
        items.addAll(list)
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
        val binding = ItemPhraseBinding.inflate(
            LayoutInflater.from(parent.context), parent, false
        )
        return VH(binding)
    }

    override fun onBindViewHolder(holder: VH, position: Int) {
        holder.binding.textPhrase.text = "${position + 1}. ${items[position]}"
        holder.binding.btnDelete.setOnClickListener { onDelete(position) }
    }

    override fun getItemCount() = items.size

    class VH(val binding: ItemPhraseBinding) : RecyclerView.ViewHolder(binding.root)
}

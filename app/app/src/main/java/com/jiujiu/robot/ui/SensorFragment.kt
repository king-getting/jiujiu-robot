package com.jiujiu.robot.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.jiujiu.robot.AppConfig.api
import com.jiujiu.robot.databinding.FragmentSensorBinding
import com.jiujiu.robot.net.ApiResult
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Sensor page: temperature / humidity / air quality, auto refresh every 2 seconds. */
class SensorFragment : Fragment() {

    private var _binding: FragmentSensorBinding? = null
    private val binding get() = _binding!!
    private var pollJob: Job? = null

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSensorBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.btnRefresh.setOnClickListener {
            viewLifecycleOwner.lifecycleScope.launch { fetchSensor() }
        }
        pollJob = viewLifecycleOwner.lifecycleScope.launch {
            while (isActive) {
                fetchSensor()
                delay(2000)
            }
        }
    }

    private suspend fun fetchSensor() {
        binding.btnRefresh.isEnabled = false
        binding.textStatus.text = "读取中..."
        when (val result = requireContext().api().readSensor()) {
            is ApiResult.Ok -> {
                binding.textTemp.text = "%.1f ℃".format(result.data.temp)
                binding.textHumi.text = "%.0f %%".format(result.data.humi)
                binding.textAir.text = "${result.data.air}"
                val time = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())
                binding.textStatus.text = "更新于 $time"
            }
            is ApiResult.Err -> binding.textStatus.text = "读取失败：${result.message}"
        }
        binding.btnRefresh.isEnabled = true
    }

    override fun onDestroyView() {
        super.onDestroyView()
        pollJob?.cancel()
        pollJob = null
        _binding = null
    }
}
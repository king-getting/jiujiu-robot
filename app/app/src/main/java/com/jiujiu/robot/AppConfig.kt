package com.jiujiu.robot

import android.content.Context
import com.jiujiu.robot.net.HttpApi
import com.jiujiu.robot.net.JiuJiuApi
import com.jiujiu.robot.net.MockApi

/**
 * 全局配置：设备 IP + Mock 开关，SharedPreferences 持久化。
 * mockMode=true 时所有网络请求走 MockApi，无硬件也能完整调试 UI。
 */
object AppConfig {

    private const val PREFS = "jiujiu_prefs"
    private const val KEY_MOCK = "mock_mode"
    private const val KEY_IP = "device_ip"
    private const val DEFAULT_IP = "192.168.1.100"

    private fun prefs(context: Context) =
        context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    var Context.mockMode: Boolean
        get() = prefs(this).getBoolean(KEY_MOCK, true)
        set(value) = prefs(this).edit().putBoolean(KEY_MOCK, value).apply()

    var Context.deviceIp: String
        get() = prefs(this).getString(KEY_IP, DEFAULT_IP) ?: DEFAULT_IP
        set(value) = prefs(this).edit().putString(KEY_IP, value).apply()

    /** 按当前模式返回通信实现，UI 层只拿这个 */
    fun Context.api(): JiuJiuApi =
        if (mockMode) MockApi() else HttpApi(deviceIp)
}

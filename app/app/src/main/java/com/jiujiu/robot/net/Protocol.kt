package com.jiujiu.robot.net

/**
 * 啾啾通信协议定义（与 docs/手机APP开发交接.md 第五节一一对应）。
 * 协议版本 ver = 1，新增指令只需新增 cmd 值，不改框架。
 */
object Protocol {
    const val VER = 1

    // cmd 值
    const val CMD_MSG = "msg"
    const val CMD_CHAT = "chat"
    const val CMD_ADD = "add"
    const val CMD_DEL = "del"
    const val CMD_WIFI = "wifi"

    // HTTP 路径
    const val PATH_MESSAGE = "/api/message"
    const val PATH_SENSOR = "/api/sensor"
    const val PATH_PHRASE_LIST = "/api/phrase/list"
    const val PATH_PHRASE_ADD = "/api/phrase/add"
    const val PATH_PHRASE_DEL = "/api/phrase/del"
    const val PATH_STATUS = "/api/status"

    const val HTTP_TIMEOUT_MS = 3_000L
    const val BLE_TIMEOUT_MS = 15_000L
}

/** 传感器数据：air 为 MQ-135 原始 ADC 值 */
data class SensorData(val temp: Double, val humi: Double, val air: Int)

/** 设备状态心跳 */
data class DeviceStatus(
    val ip: String,
    val wifi: Boolean,
    val sd: Boolean,
    val battery: Int
)

/** 统一结果封装：所有接口异常约定 {ok:false, err:"..."} */
sealed class ApiResult<out T> {
    data class Ok<T>(val data: T) : ApiResult<T>()
    data class Err(val message: String) : ApiResult<Nothing>()

    fun isOk() = this is Ok
}

/**
 * 通信层统一接口。HTTP 实现（真机）与 Mock 实现（无硬件调试）可互换，
 * UI 层只面向这个接口编程。
 */
interface JiuJiuApi {
    /** 发消息到屏幕。tts 为协议预留字段：语音模块未安装，固件端忽略即可 */
    suspend fun sendMessage(text: String, tts: Boolean): ApiResult<Unit>
    suspend fun sendChat(text: String): ApiResult<Unit>
    suspend fun readSensor(): ApiResult<SensorData>
    suspend fun phraseList(): ApiResult<List<String>>
    suspend fun phraseAdd(text: String): ApiResult<Unit>
    suspend fun phraseDel(index: Int): ApiResult<Unit>
    suspend fun status(): ApiResult<DeviceStatus>
}

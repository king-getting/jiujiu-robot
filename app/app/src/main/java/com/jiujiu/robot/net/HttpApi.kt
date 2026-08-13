package com.jiujiu.robot.net

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/**
 * HTTP 局域网实现：ESP32 为服务器，APP 直接请求。
 * 请求和响应均为 JSON，含 "ver":1 版本号；错误统一 {ok:false, err:"..."}。
 */
class HttpApi(private val deviceIp: String) : JiuJiuApi {

    private val client = OkHttpClient.Builder()
        .connectTimeout(Protocol.HTTP_TIMEOUT_MS, TimeUnit.MILLISECONDS)
        .readTimeout(Protocol.HTTP_TIMEOUT_MS, TimeUnit.MILLISECONDS)
        .writeTimeout(Protocol.HTTP_TIMEOUT_MS, TimeUnit.MILLISECONDS)
        .build()

    private val baseUrl get() = "http://$deviceIp"

    override suspend fun sendMessage(text: String, tts: Boolean): ApiResult<Unit> =
        post(Protocol.PATH_MESSAGE, JSONObject().apply {
            put("ver", Protocol.VER)
            put("cmd", Protocol.CMD_MSG)
            put("text", text)
            put("tts", tts)
        }).map { }

    override suspend fun readSensor(): ApiResult<SensorData> =
        get(Protocol.PATH_SENSOR).map { json ->
            SensorData(
                temp = json.optDouble("temp"),
                humi = json.optDouble("humi"),
                air = json.optInt("air")
            )
        }

    override suspend fun phraseList(): ApiResult<List<String>> =
        get(Protocol.PATH_PHRASE_LIST).map { json ->
            val arr = json.optJSONArray("list")
            buildList {
                if (arr != null) for (i in 0 until arr.length()) add(arr.getString(i))
            }
        }

    override suspend fun phraseAdd(text: String): ApiResult<Unit> =
        post(Protocol.PATH_PHRASE_ADD, JSONObject().apply {
            put("ver", Protocol.VER)
            put("cmd", Protocol.CMD_ADD)
            put("text", text)
        }).map { }

    override suspend fun phraseDel(index: Int): ApiResult<Unit> =
        post(Protocol.PATH_PHRASE_DEL, JSONObject().apply {
            put("ver", Protocol.VER)
            put("cmd", Protocol.CMD_DEL)
            put("index", index)
        }).map { }

    override suspend fun status(): ApiResult<DeviceStatus> =
        get(Protocol.PATH_STATUS).map { json ->
            DeviceStatus(
                ip = json.optString("ip"),
                wifi = json.optBoolean("wifi"),
                sd = json.optBoolean("sd"),
                battery = json.optInt("battery")
            )
        }

    // ---------- 内部实现 ----------

    private suspend fun get(path: String): ApiResult<JSONObject> = withContext(Dispatchers.IO) {
        runCatching {
            val request = Request.Builder().url(baseUrl + path).get().build()
            client.newCall(request).execute().use { resp ->
                if (!resp.isSuccessful) return@use ApiResult.Err("HTTP ${resp.code}")
                parseBody(resp.body?.string())
            }
        }.getOrElse { ApiResult.Err(it.message ?: "网络请求失败") }
    }

    private suspend fun post(path: String, body: JSONObject): ApiResult<JSONObject> =
        withContext(Dispatchers.IO) {
            runCatching {
                val reqBody = body.toString()
                    .toRequestBody("application/json; charset=utf-8".toMediaType())
                val request = Request.Builder().url(baseUrl + path).post(reqBody).build()
                client.newCall(request).execute().use { resp ->
                    if (!resp.isSuccessful) return@use ApiResult.Err("HTTP ${resp.code}")
                    parseBody(resp.body?.string())
                }
            }.getOrElse { ApiResult.Err(it.message ?: "网络请求失败") }
        }

    private fun parseBody(raw: String?): ApiResult<JSONObject> {
        val json = runCatching { JSONObject(raw ?: "{}") }.getOrElse {
            return ApiResult.Err("响应不是合法 JSON")
        }
        // 错误契约：{ok:false, err:"..."}
        if (json.has("ok") && !json.optBoolean("ok")) {
            return ApiResult.Err(json.optString("err", "设备返回未知错误"))
        }
        return ApiResult.Ok(json)
    }

    private inline fun <T, R> ApiResult<T>.map(transform: (T) -> R): ApiResult<R> =
        when (this) {
            is ApiResult.Ok -> ApiResult.Ok(transform(data))
            is ApiResult.Err -> this
        }
}

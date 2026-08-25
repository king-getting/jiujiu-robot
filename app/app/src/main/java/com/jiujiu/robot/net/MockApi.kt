package com.jiujiu.robot.net

import kotlinx.coroutines.delay
import kotlin.random.Random

/**
 * Mock 实现：无硬件时调试 APP 用。
 * 模拟网络延迟，返回合理假数据；鼓励语在内存里增删，行为与真机一致。
 */
class MockApi : JiuJiuApi {

    private val phrases = mutableListOf(
        "今天也要加油鸭！",
        "你超棒的，啾啾为你骄傲",
        "累了就休息一下，啾啾陪着你",
        "多喝水，少熬夜",
        "全世界你最可爱"
    )

    override suspend fun sendMessage(text: String, tts: Boolean): ApiResult<Unit> {
        delay(300)
        return if (text.isBlank()) ApiResult.Err("消息不能为空") else ApiResult.Ok(Unit)
    }

    override suspend fun sendChat(text: String): ApiResult<Unit> {
        delay(400)
        return if (text.isBlank()) ApiResult.Err("内容不能为空") else ApiResult.Ok(Unit)
    }

    override suspend fun readSensor(): ApiResult<SensorData> {
        delay(300)
        return ApiResult.Ok(
            SensorData(
                temp = 24.0 + Random.nextDouble(0.0, 4.0),
                humi = 45.0 + Random.nextDouble(0.0, 10.0),
                air = Random.nextInt(300, 900)
            )
        )
    }

    override suspend fun phraseList(): ApiResult<List<String>> {
        delay(200)
        return ApiResult.Ok(phrases.toList())
    }

    override suspend fun phraseAdd(text: String): ApiResult<Unit> {
        delay(200)
        if (text.isBlank()) return ApiResult.Err("内容不能为空")
        phrases.add(text)
        return ApiResult.Ok(Unit)
    }

    override suspend fun phraseDel(index: Int): ApiResult<Unit> {
        delay(200)
        if (index !in phrases.indices) return ApiResult.Err("序号超出范围")
        phrases.removeAt(index)
        return ApiResult.Ok(Unit)
    }

    override suspend fun status(): ApiResult<DeviceStatus> {
        delay(150)
        return ApiResult.Ok(
            DeviceStatus(ip = "192.168.1.100（模拟）", wifi = true, sd = true, battery = 87)
        )
    }
}

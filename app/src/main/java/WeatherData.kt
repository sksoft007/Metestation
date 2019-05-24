package com.konstantinsinelnik.metestation

/* ------------------------------------------------
Этот класс содержит в себе механизм парсинга строки с данными.
Строка с данными имеет формат:

<+>:<Температура по DHT11 С>:<Влажность по DHT11 %>:<Давление по BME280 мм.рт.ст.>:<Содержание СО2>:<Температура по BME280 С>:<Влажность по BME280 %>
Строка данных разделена ":". Первый символ "+" или "-" нужен только для определения начала строки и понимания, что данные обновлены.
Внутри помещения установлен датчик DHT11, а снаружи BME280

12.04.2019
 */

import android.os.Parcel
import android.os.Parcelable

data class WeatherData (val rowOfData: String?) : Parcelable {

    var temperatureHDC1080: Int = 0
    var pressureBME280: Int = 0
    var humidityHDC1080: Int = 0
    var co2: Int = 0
    var temperatureBME280: Int = 0
    var humidityBME280: Int = 0
    var signsOfUpdates: String = ""
    var tvocCCS811: Int = 0
    var checksumError = false

    constructor(parcel: Parcel) : this(parcel.readString()) {
        temperatureHDC1080 = parcel.readInt()
        pressureBME280 = parcel.readInt()
        humidityHDC1080 = parcel.readInt()
        co2 = parcel.readInt()
        temperatureBME280 = parcel.readInt()
        humidityBME280 = parcel.readInt()
        signsOfUpdates = parcel.readString() ?: ""
        tvocCCS811 = parcel.readInt()
        checksumError = false
    }

    init {

        if (noErrorOfDataTransferred(rowOfData)) {

            // Парсим строку с разделителями ":". Взято с https://javatalks.ru/topics/4747
            val ar = rowOfData?.split(":".toRegex())?.dropLastWhile { it.isEmpty() }?.toTypedArray()

            // присвоение данных полям
            if (!ar.isNullOrEmpty()) {
                //Log.d("ar", "!!! WeatherData ar.indices = "+ ar.indices)
                if (ar.isNotEmpty()) this.signsOfUpdates = ar[0] else this.signsOfUpdates = "x"
                if (ar.size >= 2) this.temperatureHDC1080 = ar[1].toIntOrNull() ?: 99 else this.temperatureHDC1080 = 99
                if (ar.size >= 3) this.humidityHDC1080 = ar[2].toIntOrNull() ?: 999 else this.humidityHDC1080 = 999
                if (ar.size >= 4) this.pressureBME280 = ar[3].toIntOrNull() ?: 999 else this.pressureBME280 = 999
                if (ar.size >= 5) this.co2 = ar[4].toIntOrNull() ?: 9999 else this.co2 = 9999
                if (ar.size >= 6) this.temperatureBME280 = ar[5].toIntOrNull() ?: 99 else this.temperatureBME280 = 99
                if (ar.size >= 7) this.humidityBME280 = ar[6].toIntOrNull() ?: 99 else this.humidityBME280 = 99
                if (ar.size >= 8) this.tvocCCS811 = ar[7].toIntOrNull() ?: 99 else this.tvocCCS811 = 99
            }
        } else checksumError = true
    }

    override fun writeToParcel(parcel: Parcel, flags: Int) {
        parcel.writeString(rowOfData)
        parcel.writeInt(temperatureHDC1080)
        parcel.writeInt(pressureBME280)
        parcel.writeInt(humidityHDC1080)
        parcel.writeInt(co2)
        parcel.writeInt(temperatureBME280)
        parcel.writeInt(humidityBME280)
        parcel.writeString(signsOfUpdates)
        parcel.writeInt(tvocCCS811)
    }

    override fun describeContents(): Int {
        return 0
    }

    private fun noErrorOfDataTransferred(receivedMessageOld: String?): Boolean {

        // Если сообщения нет, или оно пустое то принимать его нет смысла
        if (receivedMessageOld == null) return false
        if (receivedMessageOld.isEmpty()) return false

        val receivedMessage = receivedMessageOld.substringBefore('*').dropLast(1)        // подстрока до первого указанного разделителя ":*"

        var checksum = 0
        val checkSumFromArduino: String? = rowOfData?.substringAfterLast("*")


        for (x in receivedMessage.indices) run {
            checksum = checksum xor receivedMessage[x].toInt()
        }

        //Log.d("WeatherData", "checkSumFromArduino = $checkSumFromArduino checksumKotlin = ${checksum.toString(16)}")
        return checksum.toString(16) == checkSumFromArduino
    }

    companion object CREATOR : Parcelable.Creator<WeatherData> {
        override fun createFromParcel(parcel: Parcel): WeatherData {
            return WeatherData(parcel)
        }

        override fun newArray(size: Int): Array<WeatherData?> {
            return arrayOfNulls(size)
        }

    }

}


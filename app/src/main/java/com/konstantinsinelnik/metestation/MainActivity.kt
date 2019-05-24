package com.konstantinsinelnik.metestation

import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Message
import android.support.v4.content.ContextCompat
import android.util.Log
import android.widget.TextView
import android.widget.Toast
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID
import android.bluetooth.BluetoothSocket as BluetoothSocket1


class MainActivity : Activity() {

    internal var h: Handler? = null

    var txtArduino:         TextView? = null
    var temperatureHDC1080: TextView? = null
    var pressureBME280:     TextView? = null
    var humidityHDC1080:    TextView? = null
    var co2:                TextView? = null
    var signsOfUpdates:     TextView? = null
    var temperatureBME280:  TextView? = null
    var humidityBME280:     TextView? = null
    var tvocCCS811:         TextView? = null

    internal val receiveMessage: Int = 1        // Статус для Handler
    private var btAdapter: BluetoothAdapter? = null
    private var btSocket: BluetoothSocket1? = null
    private val sb = StringBuilder()

    private var mConnectedThread: ConnectedThread? = null

    /** Called when the activity is first created.  */
    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_main)

        txtArduino =        findViewById(R.id.txtArduino)
        temperatureHDC1080 =findViewById(R.id.temperatureHDC1080)
        pressureBME280 =    findViewById(R.id.pressureBME280)
        humidityHDC1080 =   findViewById(R.id.humidityHDC1080)
        co2 =               findViewById(R.id.co2)
        signsOfUpdates =    findViewById(R.id.signsOfUpdates)
        temperatureBME280 = findViewById(R.id.temperatureBME280)
        humidityBME280 =    findViewById(R.id.humidityBME280)
        tvocCCS811 =        findViewById(R.id.tvocCCS811)

        h = @SuppressLint("HandlerLeak")    // @SuppressLint("HandlerLeak") добавил компилятор
        object : Handler() {
            override fun handleMessage(msg: Message) {
                when (msg.what) {
                    receiveMessage                                                   // если приняли сообщение в Handler
                    -> {
                        val readBuf = msg.obj as ByteArray
                        val strIncom = String(readBuf, 0, msg.arg1)
                        sb.append(strIncom)                                              // формируем строку
                        val endOfLineIndex = sb.indexOf("\r\n")                        // определяем символы конца строки
                        //val endOfLineIndex = sb.indexOf("Z")                        // определяем символы конца строки
                        if (endOfLineIndex > 0) {                                        // если встречаем конец строки,
                            val sbprint = sb.substring(0, endOfLineIndex)        // то извлекаем строку
                            sb.delete(0, sb.length)                                      // и очищаем sb

                            //!!! чтение данных для вывода по значениям вмнсто вывода в txtArduino
                            val measurementResult = WeatherData(rowOfData = sbprint)
                            temperatureHDC1080?.text = "${measurementResult.temperatureHDC1080} °C"

                            pressureBME280?.text = "${measurementResult.pressureBME280} мм"
                            humidityHDC1080?.text = "${measurementResult.humidityHDC1080} %"
                            co2?.text = "СО₂ ${measurementResult.co2} ppm"
                            temperatureBME280?.text = "${measurementResult.temperatureBME280} °C"
                            humidityBME280?.text = "${measurementResult.humidityBME280} %"
                            if (measurementResult.checksumError) {
                                tvocCCS811?.text = "Check sum error"
                            } else {
                                tvocCCS811?.text = "TVOC: ${measurementResult.tvocCCS811} ppd"
                            }

                            // Меняем цвет, для визуализации
                            if (measurementResult.signsOfUpdates != "+") {
                                signsOfUpdates?.setTextColor(ContextCompat.getColor(baseContext, R.color.colorPrimaryDark)) //baseContext - это context или getBaseContext() !!!
                                signsOfUpdates?.text = "●"}
                            else {
                                signsOfUpdates?.setTextColor(ContextCompat.getColor(baseContext, R.color.colorAccent))
                                signsOfUpdates?.text = "○"}


                            txtArduino?.text = "data series: $sbprint"                           // обновляем TextView !!! для наглядности
                        }
                    }
                }//Log.d(TAG, "...Строка:"+ sb.toString() +  "Байт:" + msg.arg1 + "...");
            }
        }

        btAdapter = BluetoothAdapter.getDefaultAdapter()       // получаем локальный Bluetooth адаптер
        checkBTState()
    }

    public override fun  onResume() {
        super.onResume()

        Log.d(TAG, "...onResume - попытка соединения...")

        // Set up a pointer to the remote node using it's address.
        val device = btAdapter!!.getRemoteDevice(address)

        // Two things are needed to make a connection:
        //   A MAC address, which we got above.
        //   A Service ID or UUID.  In this case we are using the
        //     UUID for SPP.
        try {
            btSocket = device.createRfcommSocketToServiceRecord(MY_UUID)
        } catch (e: IOException) {
            errorExit("Fatal Error", "In onResume() and socket create failed: " + e.message + ".")
        }

        // Discovery is resource intensive.  Make sure it isn't going on
        // when you attempt to connect and pass your message.
        btAdapter!!.cancelDiscovery()

        // Establish the connection.  This will block until it connects.
        Log.d(TAG, "...Соединяемся...")
        try {
            btSocket!!.connect()
            Log.d(TAG, "...Соединение установлено и готово к передачи данных...")
        } catch (e: IOException) {
            try {
                btSocket!!.close()
            } catch (e2: IOException) {
                errorExit(
                    "Fatal Error",
                    "In onResume() and unable to close socket during connection failure" + e2.message + "."
                )
            }

        }

        // Create a data stream so we can talk to server.
        Log.d(TAG, "...Создание Socket...")

        mConnectedThread = ConnectedThread(btSocket!!)
        mConnectedThread!!.start()
    }

    public override fun onPause() {
        super.onPause()

        Log.d(TAG, "...In onPause()...")

        try {
            btSocket!!.close()
        } catch (e2: IOException) {
            errorExit("Fatal Error", "In onPause() and failed to close socket." + e2.message + ".")
        }

    }

    private fun checkBTState() {
        // Проверьте наличие поддержки Bluetooth, а затем убедитесь, что она включена
        // Эмулятор не поддерживает Bluetooth и возвращает null, нужно реальное устройство

        if (this.btAdapter == null) {
            errorExit("Fatal Error", "Bluetooth не поддерживается")
        } else {
            if (btAdapter!!.isEnabled) {
                Log.d(TAG, "...Bluetooth включен...")
            } else {

                // Предлагаем пользователю включить блютуз
                //val enableBtIntent = Intent(btAdapter.ACTION_REQUEST_ENABLE)  // этот вариант после автоматичской конвертации, не работает
                val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                startActivityForResult(enableBtIntent,
                    REQUEST_ENABLE_BT
                )

                // второй вариант из интернета, не пробовал https://toster.ru/q/80390
                //val enableBtIntent = BluetoothAdapter.getDefaultAdapter()
                //enableBtIntent.enable()
            }
        }
    }

    private fun errorExit(title: String, message: String) {
        Toast.makeText(baseContext, "$title - $message", Toast.LENGTH_LONG).show()
        finish()
    }

    private inner class ConnectedThread(mmSocket: BluetoothSocket1) : Thread() {
        private val mmInStream: InputStream?
        private val mmOutStream: OutputStream?

        init {
            var tmpIn: InputStream? = null
            var tmpOut: OutputStream? = null

            // Get the input and output streams, using temp objects because
            // member streams are final
            try {
                tmpIn = mmSocket.inputStream
                tmpOut = mmSocket.outputStream
            } catch (e: IOException) {
            }

            mmInStream = tmpIn
            mmOutStream = tmpOut
        }

        override fun run() {
            val buffer = ByteArray(256)  // buffer store for the stream
            var bytes: Int // bytes returned from read()

            // Keep listening to the InputStream until an exception occurs
            while (true) {
                try {
                    // Read from the InputStream
                    bytes =
                        mmInStream!!.read(buffer)        // Получаем кол-во байт и само собщение в байтовый массив "buffer"
                    h?.obtainMessage(receiveMessage, bytes, -1, buffer)?.sendToTarget()     // Отправляем в очередь сообщений Handler
                } catch (e: IOException) {
                    break
                }

            }
        }
    }

    companion object {
        private const val TAG = "bluetooth2"

        private const val REQUEST_ENABLE_BT = 1

        // SPP UUID сервиса. Взято из документации
        private val MY_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

        // MAC-адрес Bluetooth модуля. Нужно поменять на свой !!! Записал его с программы терминала
        private const val address = "98:D3:71:F5:B3:84"
    }
}
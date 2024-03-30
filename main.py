from flask import Flask, render_template, Response, request
import cv2
import serial
import threading
import time
import json
import argparse

def is_json(myjson):
  try:
    json.loads(myjson)
  except ValueError as e:
    return False
  return True


app = Flask(__name__)
camera = cv2.VideoCapture(0)
led_port = "COM13"

controlX, controlY = 0, 0  # глобальные переменные положения джойстика с web-страницы

face_cascade = cv2.CascadeClassifier('haarcascade_frontalface_default.xml')

video = cv2.VideoCapture(0)

msg_led = {
    "red": 0,
    "green": 0,
    "blue": 0
}
msg_chassis = {
   "speed_l": 0,
    "speed_r": 0
}
chassis_answer = {}
radsens_answer = {}

maxAbsSpeed = 255
speedScale = 1
def getFramesGenerator():
    """ Генератор фреймов для вывода в веб-страницу, тут же можно поиграть с openCV"""
    while True:
        # time.sleep(0.01)  # ограничение fps (если видео тупит, можно убрать)
        success, frame = camera.read()  # Получаем фрейм с камеры

        if success:
            frame = cv2.flip(frame, 1)
            frame = cv2.resize(frame, (320, 240),
                               interpolation=cv2.INTER_AREA)  # уменьшаем разрешение кадров (если видео тупит, можно уменьшить еще больше)
            height, width = frame.shape[0:2]  # получаем разрешение кадра
            # faces = face_cascade.detectMultiScale(frame, scaleFactor=1.1, minNeighbors=10)

            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)  # переводим кадр из RGB в HSV

            # binary = cv2.inRange(hsv, (18, 60, 100), (32, 255, 255))  # пороговая обработка кадра (выделяем все желтое)
            #binary = cv2.inRange(hsv, (0, 0, 0), (255, 255, 35))  # пороговая обработка кадра (выделяем все черное)

            bin1 = cv2.inRange(hsv, (0, 60, 70), (10, 255, 255)) # красный цвет с одного конца
            bin2 = cv2.inRange(hsv, (160, 60, 70), (179, 255, 255)) # красный цвет с другого конца
            binary = bin1 + bin2  # складываем битовые маски


            contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL,                               cv2.CHAIN_APPROX_NONE)  # получаем контуры выделенных областей

            if len(contours) != 0:  # если найден хоть один контур
                maxc = max(contours, key=cv2.contourArea)  # находим наибольший контур
                moments = cv2.moments(maxc)  # получаем моменты этого контура
                """
                # moments["m00"] - нулевой момент соответствует площади контура в пикселях,
                # поэтому, если в битовой маске присутствуют шумы, можно вместо
                # if moments["m00"] != 0:  # использовать

                if moments["m00"] > 20: # тогда контуры с площадью меньше 20 пикселей не будут учитываться 
                """
                if moments["m00"] > 20:  # контуры с площадью меньше 20 пикселей не будут учитываться
                    cx = int(moments["m10"] / moments["m00"])  # находим координаты центра контура по x
                    cy = int(moments["m01"] / moments["m00"])  # находим координаты центра контура по y

                    iSee = True  # устанавливаем флаг, что контур найден

                    controlX = 2 * (cx - width / 2) / width  # находим отклонение найденного объекта от центра кадра и
                    # нормализуем его (приводим к диапазону [-1; 1])

                    cv2.drawContours(frame, maxc, -1, (0, 255, 0), 1)  # рисуем контур
                    cv2.line(frame, (cx, 0), (cx, height), (0, 255, 0), 1)  # рисуем линию линию по x
                    cv2.line(frame, (0, cy), (width, cy), (0, 255, 0), 1)  # линия по y

            # frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)   # перевод изображения в градации серого
           # _, frame = cv2.threshold(frame, 127, 255, cv2.THRESH_BINARY)  # бинаризуем изображение
           #   print(len(faces))
           #
           #  if len(faces) > 1:
           #      msg_led["red"], msg_led["green"], msg_led["blue"] = 0, 0, 25
           #  elif len(faces) > 0:
           #      msg_led["red"], msg_led["green"], msg_led["blue"] = 25, 0, 0
           #  else:
           #      msg_led["red"], msg_led["green"], msg_led["blue"] = 0, 25, 0

            _, buffer = cv2.imencode('.jpg', frame)
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')


def shut_down(really):
    if really!="true":
        return
    print("shutting down")
    command = "/usr/bin/sudo /sbin/shutdown -h now"
    import subprocess
    process = subprocess.Popen(command.split(), stdout=subprocess.PIPE)
    output = process.communicate()[0]
    print(output)


@app.route('/video_feed')
def video_feed():
    """ Генерируем и отправляем изображения с камеры"""
    return Response(getFramesGenerator(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/')
def index():
    """ Крутим html страницу """
    return render_template('index.html')

@app.route('/control')
def control():
    """ Пришел запрос на управления роботом """
    global controlX, controlY
    controlX, controlY = float(request.args.get('x')) / 100.0, float(request.args.get('y')) / 100.0
    return '', 200, {'Content-Type': 'text/plain'}


@app.route('/data')
def data():
    """ Пришел запрос на управления роботом """
    global radsens_answer, chassis_answer
    _data = radsens_answer
    return _data, 200, {'Content-Type': 'application/json'}


@app.route('/device')
def device():
    """ Пришел запрос на выключение малинки """
    shut_down(request.args.get('poweroff'))
    return '', 200, {'Content-Type': 'text/plain'}


if __name__ == '__main__':

    sendFreq = 10  # слать 10 пакетов в секунду
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', type=int, default=5000, help="Running port")
    parser.add_argument("-i", "--ip", type=str, default='0.0.0.0', help="Ip address")
    parser.add_argument('-cs', '--chassisserial', type=str, default='COM14', help="Chassis serial port")
    parser.add_argument('-rs', '--radsensserial', type=str, default='COM11', help="Radsens serial port")
    args = parser.parse_args()

    chassisSerialPort = serial.Serial(args.chassisserial, 115200)
    radsensSerialPort = serial.Serial(args.radsensserial, 115200)

    def sender():
        """ функция цикличной отправки пакетов по uart """
        global msg_chassis, chassis_answer, radsens_answer
        while True:
            speed_l = maxAbsSpeed * (controlY + controlX)  # преобразуем скорость робота,
            speed_r = maxAbsSpeed * (controlY - controlX)  # в зависимости от положения джойстика

            speed_l = max(-maxAbsSpeed, min(speed_l, maxAbsSpeed))  # функция аналогичная constrain в arduino
            speed_r = max(-maxAbsSpeed, min(speed_r, maxAbsSpeed))  # функция аналогичная constrain в arduino

            msg_chassis["speed_l"], msg_chassis["speed_r"] = int(speedScale * speed_l), int(speedScale * speed_r)  # урезаем скорость и упаковываем
            print("x, y:", controlX, controlY)

            chassisSerialPort.write(json.dumps(msg_chassis, ensure_ascii=False).encode("utf8"))  # отправляем пакет в виде json файла
            print(json.dumps(msg_chassis, ensure_ascii=False).encode("utf8"))

            _answer = chassisSerialPort.readline()
            if is_json(_answer):
                chassis_answer = _answer

            _answer = radsensSerialPort.readline()
            if is_json(_answer):
                radsens_answer = _answer
            time.sleep(1 / sendFreq)

threading.Thread(target=sender, daemon=True).start()  # запускаем тред отправки пакетов по uart с демоном

app.run(debug=False, host=args.ip, port=args.port)  # запускаем flask приложение

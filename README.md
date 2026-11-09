# proj_stm32_elevator

STM32F411RE(NUCLEO-F411RE)로 만든 **3층 2대 엘리베이터 제어 시스템**입니다.
스텝모터 2개가 각각 엘리베이터 한 대를 움직이고, 포토인터럽터로 층 위치를 감지합니다.
층 호출은 보드의 버튼이나 PC 프로그램(UART)으로 할 수 있고, 두 대 중 더 가까운 엘리베이터가 배정됩니다.

## 동작 영상

https://github.com/user-attachments/assets/9caccf39-1486-453f-8862-f22152f4ac2f

## 주요 기능

- **2대 엘리베이터 동시 제어**: 스텝모터 2개를 논블로킹 방식으로 따로 구동합니다.
- **가까운 엘리베이터 배정**: 거리와 진행 방향으로 비용을 계산해 호출을 배정합니다.
- **호출 취소**: 같은 층 버튼을 한 번 더 누르면 호출이 취소됩니다.
- **층 감지**: 층마다 포토인터럽터를 두고 EXTI 인터럽트로 현재 층을 갱신합니다.
- **상태 표시**
  - I2C 16x2 LCD에 두 엘리베이터의 방향과 현재 층을 표시합니다 (예: `RUN ^2F  STOP 1F`).
  - 층별 LED는 해당 층에 호출이 걸려 있는 동안 켜집니다.
  - 엘리베이터가 도착하면 부저가 울립니다.
- **UART 프로토콜**: PC에서 층 호출과 상태 조회를 할 수 있습니다.
- **PC 제어 프로그램**: Tkinter GUI이며, 보드 없이 돌려 볼 수 있는 데모 모드가 있습니다.

## 하드웨어 구성

| 구분 | 부품 | 핀 |
|---|---|---|
| MCU | STM32F411RET6 (100 MHz) | — |
| 엘리베이터 1 모터 | 28BYJ-48 + ULN2003 | PC10(IN1), PC11(IN2), PC12(IN3), PD2(IN4) |
| 엘리베이터 2 모터 | 28BYJ-48 + ULN2003 | PC7(IN1), PB6(IN2), PA7(IN3), PA6(IN4) |
| 엘리베이터 1 층 센서 | 포토인터럽터 ×3 (EXTI) | 1F: PC5, 2F: PC6, 3F: PC9 |
| 엘리베이터 2 층 센서 | 포토인터럽터 ×3 (EXTI) | 1F: PB4, 2F: PB10, 3F: PA8 |
| 호출 버튼 | 택트 스위치 ×3 | 1F: PC0, 2F: PC1, 3F: PC3 |
| 호출 LED | LED ×3 | 1F: PB13, 2F: PB14, 3F: PB15 |
| LCD | 16x2 Character LCD (I2C, PCF8574) | SCL: PB8, SDA: PB9 (I2C1) |
| 부저 | 패시브 부저 (PWM) | PA11 (TIM1_CH4) |
| UART | ST-LINK 가상 COM 포트 | PA2(TX), PA3(RX) — USART2, 115200 bps |

> 핀 설정 전체는 `proj_stm32_elevator.ioc`(STM32CubeMX)에서 볼 수 있습니다.

## 프로젝트 구조

```
proj_stm32_elevator/
├── Core/
│   ├── Inc/                      # CubeMX 생성 헤더
│   └── Src/
│       ├── main.c                # HAL/주변장치 초기화 → ap_main() 호출
│       ├── ap/
│       │   ├── ap_main.c         # 메인 루프, EXTI/UART 콜백, 패킷 명령 처리
│       │   └── service/
│       │       ├── Elevator/     # 엘리베이터 로직 (배정, 이동, 취소, 표시)
│       │       └── sound/        # 부저 효과음
│       ├── driver/
│       │   ├── button/           # 버튼 (에지 검출)
│       │   ├── buzzer/           # PWM 부저
│       │   ├── lcd/              # I2C LCD
│       │   ├── led/              # LED
│       │   ├── stepmotor/        # 스텝모터 하프스텝 구동
│       │   └── uart_protocol/    # 패킷 송수신, 파서
│       └── common/delay/         # 딜레이 유틸
├── Drivers/                      # STM32 HAL / CMSIS
├── pc_app/
│   ├── elevator_control.py       # PC 제어 GUI
│   └── requirements.txt
└── proj_stm32_elevator.ioc       # CubeMX 설정 파일
```

### 소프트웨어 구조

`main.c` → `ap_main()`에서 슈퍼루프가 돕니다.

```c
while (1) {
    uart_protocol_task();   // 수신 완료된 패킷 처리
    elevator_task();        // 버튼 → 배정 → 방향 결정 → 모터 스텝 → LCD/LED 갱신
}
```

- **인터럽트**: 층 센서(EXTI)는 현재 층만 갱신하고 도착 이벤트 플래그를 세웁니다. UART RX는 1바이트씩 받아서 파서에 넘깁니다.
- 인터럽트 안에서 무거운 일을 하지 않도록, 실제 처리는 메인 루프에서 합니다.
- 모터는 `HAL_GetTick()`으로 2 ms마다 한 스텝씩 움직이므로 두 대가 서로 막지 않고 동시에 움직입니다.

## 엘리베이터 동작

### 상태

엘리베이터마다 아래 값을 가집니다.

| 필드 | 설명 |
|---|---|
| `curFloor` | 마지막으로 지난 센서의 층 |
| `dir` | `DIR_IDLE`(0) / `DIR_UP`(1) / `DIR_DOWN`(2) |
| `reqMask` | 배정된 호출 층 비트마스크 (bit1=1F, bit2=2F, bit3=3F) |
| `forcedMask` | 호출이 취소된 뒤 이동 중인 엘리베이터를 세우기 위한 임시 정차 층 |

### 호출 배정 (`elevator_select`)

1. 이미 배정된 층을 다시 호출하면 **호출을 취소**합니다.
2. 임시 정차 층(`forcedMask`)을 호출하면 정식 호출로 바꿉니다.
3. 멈춰 있는 엘리베이터가 이미 그 층에 있으면 배정하지 않습니다.
4. 나머지 경우에는 두 엘리베이터의 비용을 계산해 비용이 낮은 쪽에 배정합니다. 비용이 같으면 1호기에 배정합니다.
   - 비용 = |현재 층 − 호출 층|
   - 호출 층이 진행 방향의 반대쪽에 있으면 **+3** 페널티

### 방향 결정

- 올라가는 중에 위쪽 호출이 없으면, 또는 내려가는 중에 아래쪽 호출이 없으면 정지합니다.
- 정지한 상태에서는 위쪽 호출을 먼저 처리하고, 위쪽 호출이 없을 때 아래쪽 호출을 처리합니다.
- 호출된 층의 센서를 지나면 도착으로 보고 호출을 지우고 부저를 울립니다.

### 호출 취소 (`elevator_cancel`)

이동 중에 목적지 호출이 취소되면, 같은 방향의 다음 층을 임시 정차 층으로 정해 그 층에서 멈추게 합니다.
같은 방향에 새 호출이 들어오면 임시 정차는 해제됩니다.

## UART 프로토콜

- 설정: **115200 bps, 8N1** (USART2, ST-LINK VCP)
- 패킷 형식

```
[0xAA][LEN][CMD][PAYLOAD ...][CHK]

LEN = 1 + payload 길이   (CMD + PAYLOAD 바이트 수)
CHK = (LEN + CMD + sum(PAYLOAD)) & 0xFF
```

### 명령 (PC → MCU)

| CMD | 이름 | Payload | 설명 |
|---|---|---|---|
| `0x10` | `REQ_FLOOR_1` | 없음 | 1층 호출 (호출 중이면 취소) |
| `0x11` | `REQ_FLOOR_2` | 없음 | 2층 호출 (호출 중이면 취소) |
| `0x12` | `REQ_FLOOR_3` | 없음 | 3층 호출 (호출 중이면 취소) |
| `0x20` | `REQ_ELEVATOR_STATE` | 없음 | 두 엘리베이터 상태 조회 |

### 응답 (MCU → PC)

응답 CMD는 **요청 CMD + `0x80`** 입니다.

| CMD | Payload |
|---|---|
| `0x90` ~ `0x92` | 1 byte: `0x00`(OK) / `0x01`(ERROR) |
| `0xA0` | 6 bytes: `[E1 층][E1 방향][E1 reqMask][E2 층][E2 방향][E2 reqMask]` |

체크섬이 틀리면 `요청 CMD + 0x80`으로 `0x01`(ERROR)을 돌려줍니다.

### 예시

```
2층 호출   PC → MCU : AA 01 11 12
           MCU → PC : AA 02 91 00 93

상태 조회  PC → MCU : AA 01 20 21
           MCU → PC : AA 07 A0 02 01 08 01 00 00 B3
                      └ E1: 2층, 상승 중, 3층 호출 / E2: 1층, 정지, 호출 없음
```

## 빌드 및 실행

### 펌웨어

1. **STM32CubeIDE**에서 `File → Open Projects from File System...`으로 이 폴더를 엽니다.
2. 빌드(`Ctrl+B`)한 뒤 NUCLEO 보드를 연결하고 `proj_stm32_elevator Debug.launch`로 다운로드/디버그합니다.

### PC 제어 프로그램

Python 3.8 이상이 필요합니다. (GUI는 Python에 포함된 Tkinter를 사용합니다.)

```bash
cd pc_app
pip install -r requirements.txt

python elevator_control.py          # 보드 연결: COM 포트를 고른 뒤 연결
python elevator_control.py --demo   # 보드 없이 시뮬레이션
```

- 건물 화면에서 층을 클릭하면 호출되고, 다시 클릭하면 취소됩니다.
- 200 ms마다 상태를 조회해 두 엘리베이터의 위치와 방향, 호출 상태를 화면에 표시합니다. (자동 갱신은 끌 수 있습니다.)
- 주고받은 패킷은 PACKET LOG에 HEX로 표시됩니다.
- 데모 모드는 펌웨어의 배정/취소 로직을 그대로 흉내 내므로 하드웨어 없이 프로토콜과 동작을 확인할 수 있습니다.

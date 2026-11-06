# 엘리베이터 층 호출 LED (reqMask 활용)

## 동작

| 상황 | 코드에서 일어나는 일 | LED |
| --- | --- | --- |
| 층 버튼 누름 | `elevator_select()` → `reqMask \|= bit` | ON |
| 같은 버튼 다시 누름 (취소) | `elevator_cancel()` → `reqMask &= ~bit` | OFF |
| 해당 층 도착 | `elevator_task()` → `reqMask &= ~bit` | OFF |

- LED를 따로 켜고 끄지 않음 → **`reqMask`를 그대로 LED에 비춤**
- 버튼 / 취소 / 도착 처리는 이미 `reqMask`를 바꾸고 있음 → LED는 그 결과만 보면 됨

## 핀 (CubeMX: GPIO_Output)

| 층 | 핀 |
| --- | --- |
| 1F | PB13 |
| 2F | PB14 |
| 3F | PB15 |

## reqMask 비트

| 비트 | 7~4 | 3 | 2 | 1 | 0 |
| --- | --- | --- | --- | --- | --- |
| 의미 | 안 씀 | 3F | 2F | 1F | 안 씀 |

- 1F + 3F 호출 → `0b0000 1010` = 10

## 핵심 포인트

### 두 엘리베이터 호출 합치기 → `|`

| 연산자 | `0b0010` ○ `0b1000` | 판단 |
| --- | --- | --- |
| `&` | `0b0000` | 둘 다 있어야 켜짐 → X |
| `\|` | `0b1010` | 하나라도 있으면 켜짐 → O |
| `^` | `0b1010` | 같은 비트가 둘 다 1이면 꺼짐 → 의도와 다름 |

### 특정 층 확인 → `calls & (1 << f)`

| 식 | 2진수 | 층 |
| --- | --- | --- |
| `1 << 1` | `0b0010` | 1F |
| `1 << 2` | `0b0100` | 2F |
| `1 << 3` | `0b1000` | 3F |

- `1 << n` = n번 비트만 1 → `&` 하면 그 비트만 남음 → 0이 아니면 호출 있음

### 배열을 `ledFloor[4]`로 선언한 이유

- `ledFloor[3]`이면 인덱스 0~2 → `ledFloor[3]` 접근 시 범위 밖
- 0번 칸을 비워서 **층 번호 = 비트 번호 = 인덱스**로 통일 → `f` 하나로 처리

### 호출 위치 → `elevator_task()` 끝에서 매 루프

- 버튼 / 취소 / 도착마다 LED 코드를 넣으면 한 곳만 빠져도 LED가 실제 호출 상태와 어긋남
- 매 루프 `reqMask`를 보고 갱신 → 나중에 호출을 바꾸는 코드가 추가돼도 LED 코드는 수정할 필요 없음

### forcedMask는 제외

- 취소 후 엘리베이터가 대신 서는 층 → 사람이 누른 호출이 아니라서 LED에 안 넣음

## 코드 (elevator.c)

```c
#include "../../../driver/led/led.h"

led_t ledFloor[4];   // index 1~3 = floor (0 unused)

void elevator_init() {
	// ... existing code ...
	led_init(&ledFloor[1], GPIOB, GPIO_PIN_13);   // 1F
	led_init(&ledFloor[2], GPIOB, GPIO_PIN_14);   // 2F
	led_init(&ledFloor[3], GPIOB, GPIO_PIN_15);   // 3F
}

void elevator_ledTask() {
	// hall calls of both elevators (forcedMask excluded)
	uint8_t calls = elevators[0].reqMask | elevators[1].reqMask;

	for (int f = 1; f <= 3; f++) {
		if (calls & (1 << f)) led_on(&ledFloor[f]);
		else                  led_off(&ledFloor[f]);
	}
}

void elevator_task() {
	// ... existing code ...
	elevator_display();
	elevator_ledTask();   // update LEDs every loop
}
```

## 확인

- 버튼 누름 → 해당 층 LED ON
- 같은 버튼 다시 누름 → OFF
- 엘리베이터가 그 층 도착 → OFF
- 이미 그 층에 서 있는 엘리베이터가 있을 때 누름 → 호출이 안 들어가서 LED도 안 켜짐 (정상)

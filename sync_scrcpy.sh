#!/bin/bash

########################################################################
# # 스크립트에 실행 권한 부여
# chmod +x sync_scrcpy.sh

# # 특정 소스 기기에서 특정 대상 기기로 이벤트 전달
# ./sync_scrcpy.sh -s SOURCE_SERIAL -t TARGET_SERIAL1,TARGET_SERIAL2

# # 특정 소스 기기에서 다른 모든 연결된 기기로 이벤트 전달
# ./sync_scrcpy.sh -s SOURCE_SERIAL
########################################################################


# 사용법 출력 함수
usage() {
    echo "사용법: $0 -s SOURCE_SERIAL [-t TARGET_SERIAL1,TARGET_SERIAL2,...]"
    echo "옵션:"
    echo "  -s: 이벤트 소스가 될 기기의 시리얼 번호"
    echo "  -t: 이벤트를 전달받을 대상 기기들의 시리얼 번호 (쉼표로 구분)"
    exit 1
}

# 명령행 인자 파싱
while getopts "s:t:" opt; do
    case $opt in
        s) SOURCE_SERIAL="$OPTARG";;
        t) TARGET_SERIALS="$OPTARG";;
        ?) usage;;
    esac
done

# 필수 인자 확인
if [ -z "$SOURCE_SERIAL" ]; then
    echo "에러: 소스 기기 시리얼 번호(-s)는 필수입니다."
    usage
fi

# 대상 기기가 지정되지 않은 경우 연결된 모든 기기를 대상으로 설정
if [ -z "$TARGET_SERIALS" ]; then
    TARGET_SERIALS=$(adb devices | grep -v "List" | grep -v "^$" | cut -f1 | grep -v "$SOURCE_SERIAL" | tr '\n' ',' | sed 's/,$//')
fi

# 키 매핑 함수 - SDL 키 이름을 Android 키코드로 변환
get_keycode() {
    local key=$1
    case $key in
        "Return") echo "KEYCODE_ENTER";;
        "Backspace") echo "KEYCODE_DEL";;
        "Space") echo "KEYCODE_SPACE";;
        "Left") echo "KEYCODE_DPAD_LEFT";;
        "Right") echo "KEYCODE_DPAD_RIGHT";;
        "Up") echo "KEYCODE_DPAD_UP";;
        "Down") echo "KEYCODE_DPAD_DOWN";;
        "Tab") echo "KEYCODE_TAB";;
        "Escape") echo "KEYCODE_ESCAPE";;
        "Delete") echo "KEYCODE_FORWARD_DEL";;
        "Home") echo "KEYCODE_HOME";;
        "End") echo "KEYCODE_MOVE_END";;
        "PageUp") echo "KEYCODE_PAGE_UP";;
        "PageDown") echo "KEYCODE_PAGE_DOWN";;
        [A-Z]) echo "KEYCODE_$(echo $key)";;
        [a-z]) echo "KEYCODE_$(echo $key | tr '[:lower:]' '[:upper:]')";;
        [0-9]) echo "KEYCODE_$key";;
        ",") echo "KEYCODE_COMMA";;
        ".") echo "KEYCODE_PERIOD";;
        "/") echo "KEYCODE_SLASH";;
        "\\") echo "KEYCODE_BACKSLASH";;
        ";") echo "KEYCODE_SEMICOLON";;
        "'") echo "KEYCODE_APOSTROPHE";;
        "[") echo "KEYCODE_LEFT_BRACKET";;
        "]") echo "KEYCODE_RIGHT_BRACKET";;
        "-") echo "KEYCODE_MINUS";;
        "=") echo "KEYCODE_EQUALS";;
        # "`") echo "KEYCODE_GRAVE";;
        *) echo "";;
    esac
}

# 이벤트를 다른 기기로 전달하는 함수
forward_event() {
    local event_type=$1
    shift
    
    # 쉼표로 구분된 대상 기기들을 배열로 변환
    IFS=',' read -ra TARGETS <<< "$TARGET_SERIALS"
    
    for serial in "${TARGETS[@]}"; do
        case $event_type in
            "Mouse")
                local x=$1
                local y=$2
                local touch_type=$3
                case $touch_type in
                    "Mouse Button Down"|"Mouse Move")
                        adb -s "$serial" shell input touchscreen swipe $x $y $x $y 100
                        ;;
                esac
                ;;
            "Keyboard")
                local key=$1
                local key_type=$2
                local modifiers=$3
                
                # 키코드 가져오기
                local keycode=$(get_keycode "$key")
                if [ -n "$keycode" ]; then
                    # 수정자 키 처리
                    local modifier_keys=""
                    if [[ $modifiers == *"Ctrl+"* ]]; then
                        modifier_keys="$modifier_keys --longpress KEYCODE_CTRL_LEFT"
                    fi
                    if [[ $modifiers == *"Shift+"* ]]; then
                        modifier_keys="$modifier_keys --longpress KEYCODE_SHIFT_LEFT"
                    fi
                    if [[ $modifiers == *"Alt+"* ]]; then
                        modifier_keys="$modifier_keys --longpress KEYCODE_ALT_LEFT"
                    fi
                    
                    if [ "$key_type" = "Key Down" ]; then
                        if [ -n "$modifier_keys" ]; then
                            adb -s "$serial" shell "input keyevent $modifier_keys $keycode"
                        else
                            adb -s "$serial" shell "input keyevent $keycode"
                        fi
                    fi
                fi
                ;;
        esac
    done
}

echo "source: $SOURCE_SERIAL"
echo "destination: $TARGET_SERIALS"

# source 기기의 scrcpy 실행 및 로그 모니터링
scrcpy -s "$SOURCE_SERIAL" 2>&1 | while IFS= read -r line; do
    # parse touch event log
    echo "$line"
    if [[ $line =~ "Mouse Event:" ]]; then
        if [[ $line =~ x=([0-9]+),\ y=([0-9]+) ]]; then
            x="${BASH_REMATCH[1]}"
            y="${BASH_REMATCH[2]}"
            
            if [[ $line =~ "(Mouse Button Down|Mouse Button Up|Mouse Move)" ]]; then
                touch_type=$(echo "$line" | grep -o "Mouse Button Down\|Mouse Button Up\|Mouse Move")
                forward_event "Mouse" "$x" "$y" "$touch_type"
            fi
        fi
    # parse keyboard event
    elif [[ $line =~ "Keyboard Event:" ]]; then
        # 예: "키보드 이벤트: Key Down (Ctrl+Shift+A)"
        if [[ $line =~ "Keyboard Event: (Key Down|Key Up) \((.*)\)" ]]; then
            key_type=$(echo "$line" | grep -o "Key Down\|Key Up")
            key_info=$(echo "$line" | grep -o "(.*)" | tr -d "()")
            
            # 수정자 키와 실제 키 분리
            modifiers=$(echo "$key_info" | grep -o "Ctrl+\|Shift+\|Alt+" | tr -d '\n')
            key=$(echo "$key_info" | sed 's/Ctrl+//g' | sed 's/Shift+//g' | sed 's/Alt+//g')
            
            forward_event "Keyboard" "$key" "$key_type" "$modifiers"
        fi
    fi
done
#!/bin/sh
# T92P board adapter, run as root by its init script. The application only
# T92P 板级适配脚本由初始化脚本以 root 执行；应用仅获得白光通道的写权限。
# receives write access to this one white-light duty/enable pair.
# 应用仅能写入此白光通道的 duty/enable 文件。
set -eu
light_pwm=/sys/class/pwm/pwmchip0/pwm0
light_chip=/sys/class/pwm/pwmchip0

board_check() {
    [ "$(readlink -f "$light_chip/device")" = /sys/devices/platform/20700000.pwm ]
    [ -e /proc/device-tree/sys-light-pwm/compatible ]
    grep -q pwm-light /proc/device-tree/sys-light-pwm/compatible
    # Check the vendor IR pin offset/polarity before touching GPIO0_D0.
    # 操作 GPIO0_D0 前先核对厂商红外引脚偏移与极性。
    case "$(od -An -tx1 /proc/device-tree/sys-dev/ir_led_pin | tr -d ' \n')" in
        ????????0000001800000000) ;;
        *) return 1 ;;
    esac
}

white_off() {
    if [ -d "$light_pwm" ]; then
        echo 0 > "$light_pwm/duty_cycle"
        echo 0 > "$light_pwm/enable"
    fi
}

ir_off() {
    [ -d /sys/class/gpio/gpio24 ] || echo 24 > /sys/class/gpio/export
    echo low > /sys/class/gpio/gpio24/direction
}

rgb_off() {
    # Vendor rgb_led_set_ctrl() drives the selected color high and the other
    # 厂商 rgb_led_set_ctrl() 将选中颜色拉高，其他颜色拉低。
    # colors low. All three low is off; no relay/door GPIO is touched.
    # 三个颜色全低即关闭；不操作继电器或门控 GPIO。
    for entry in rgb_red_pin:200:0000000800000001 rgb_green_pin:202:0000000a00000001 rgb_blue_pin:203:0000000b00000000; do
        pin_name=${entry%%:*}; remaining=${entry#*:}
        gpio=${remaining%%:*}; suffix=${remaining#*:}
        raw=$(od -An -tx1 "/proc/device-tree/sys-dev/$pin_name" | tr -d ' \n')
        [ "${raw#????????}" = "$suffix" ] || return 1
        [ -d "/sys/class/gpio/gpio$gpio" ] || echo "$gpio" > /sys/class/gpio/export
        echo low > "/sys/class/gpio/gpio$gpio/direction"
    done
}

case "${1:-status}" in
    prepare)
        board_check || { echo 'Unsupported white/IR light wiring' >&2; exit 1; }
        ir_off
        rgb_off
        [ -d "$light_pwm" ] || echo 0 > "$light_chip/export"
        echo 1000000 > "$light_pwm/period"
        white_off
        echo normal > "$light_pwm/polarity"
        chown root:video "$light_pwm/duty_cycle" "$light_pwm/enable"
        chmod 660 "$light_pwm/duty_cycle" "$light_pwm/enable"
        # Grant access only to the named color sensor, not every sensor or GPIO.
        # 仅授权指定颜色传感器，不开放全部传感器或 GPIO。
        for sensor in /sys/class/video4linux/v4l-subdev*; do
            if grep -q cv2002_720 "$sensor/name"; then
                chown root:video "/dev/${sensor##*/}"
                chmod 660 "/dev/${sensor##*/}"
            fi
        done
        ;;
    off)
        board_check
        white_off
        ir_off
        rgb_off
        ;;
    status)
        if [ -d "$light_pwm" ]; then
            for item in period duty_cycle enable; do
                printf 'white_%s=' "$item"; cat "$light_pwm/$item"
            done
        else echo white_pwm=unconfigured; fi
        if [ -e /sys/class/gpio/gpio24/value ]; then
            printf 'ir_gpio='; cat /sys/class/gpio/gpio24/value
        else echo ir_gpio=unconfigured; fi
        ;;
    *) echo 'Usage: t92p-lighting.sh {prepare|off|status}' >&2; exit 2 ;;
esac

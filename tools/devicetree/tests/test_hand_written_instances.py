# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

import re
import shutil
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import dtmap
import fdt

REPO = Path(__file__).resolve().parents[3]

PWM_STM32 = "tests/drivers/pwm_stm32/include/devicetree/instances"
PWM_STM32_BOOT = "tests/drivers/pwm_stm32_boot/include/devicetree/instances"

HAND_WRITTEN = [
    ("snowy", f"{PWM_STM32}/pebble_pwm_hbridge_vibrator.inc"),
    ("snowy", f"{PWM_STM32}/pwm_backlight.inc"),
    ("snowy", f"{PWM_STM32}/st_stm32f429_pinctrl_pinctrl.inc"),
    ("snowy", f"{PWM_STM32_BOOT}/st_stm32_pwm.inc"),
    ("snowy", f"{PWM_STM32_BOOT}/st_stm32f429_pinctrl_gpio.inc"),
]

WITHOUT_HOG = [
    ("snowy", f"{PWM_STM32}/st_stm32_pwm.inc"),
    ("snowy", f"{PWM_STM32}/st_stm32f429_pinctrl_gpio.inc"),
]


def without_hog(statement):
    statement = re.sub(r",PBL_GPIO_STM32_HOG\([^()]*\)", "", statement)
    return statement.replace(",&dt_gpiof.dev)", ")")


def statements(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = "\n".join(line for line in text.splitlines() if not line.startswith("#"))
    return [re.sub(r"\s+", "", s) for s in text.split(";") if s.strip()]


@pytest.fixture(scope="module")
def generated(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if cc is None or shutil.which("dtc") is None:
        pytest.skip("needs cc and dtc")
    out = {}
    for board in sorted({board for board, _ in HAND_WRITTEN + WITHOUT_HOG}):
        dtb, _, _ = dtmap.configure(
            REPO, tmp_path_factory.mktemp(board), board, None, cc
        )
        tree = fdt.load(dtb)
        specs = dtmap.load_specs(REPO)
        config = {
            spec.config
            for node in tree.nodes()
            if dtmap.enabled(node)
            for compatible in node.compatibles
            for spec in specs.get(compatible, [])
        }
        gen = dtmap.Generator(tree, specs, config)
        gen.run()
        out[board] = gen.instances()
    return out


@pytest.mark.parametrize(
    ("board", "path"), HAND_WRITTEN, ids=[p for _, p in HAND_WRITTEN]
)
def test_hand_written_instances_are_the_generators(generated, board, path):
    name = f"devicetree/instances/{Path(path).name}"
    expected = statements(generated[board][name])
    written = statements((REPO / path).read_text())
    assert written
    for statement in written:
        assert statement in expected


@pytest.mark.parametrize(
    ("board", "path"), WITHOUT_HOG, ids=[p for _, p in WITHOUT_HOG]
)
def test_hand_written_instances_are_the_generators_without_the_hog(
    generated, board, path
):
    name = f"devicetree/instances/{Path(path).name}"
    expected = [without_hog(s) for s in statements(generated[board][name])]
    written = statements((REPO / path).read_text())
    assert written
    for statement in written:
        assert statement in expected

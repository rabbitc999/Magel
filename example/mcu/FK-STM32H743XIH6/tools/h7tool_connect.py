"""通过 STM32H743 的 AP2 恢复调试时钟域，并复核 AP0 访问。"""
import time


def install_dbg_ap2_access():
    """让 pyOCD 早期连接通过官方 AP2 地址访问 DBGMCU，保留原有芯片身份校验。"""
    from pyocd.coresight.minimal_mem_ap import MinimalMemAP
    if getattr(MinimalMemAP, '_fk_ap2_access', False):
        return
    original_read = MinimalMemAP.read32
    original_write = MinimalMemAP.write32

    def read(self, address):
        """DBGMCU 的 ID/控制寄存器使用 AP2 镜像，其他访问沿用默认 MEM-AP。"""
        if address in (0x5c001000, 0x5c001004):
            self.dp.write_ap(0x02000000, 0x03000002)
            self.dp.write_ap(0x02000004, 0xe00e1000 + address - 0x5c001000)
            return self.dp.read_ap(0x0200000c)
        return original_read(self, address)

    def write(self, address, value):
        """仅将 DBGMCU 控制寄存器转发至 AP2，不改变 Flash 或普通内存访问。"""
        if address == 0x5c001004:
            self.dp.write_ap(0x02000000, 0x03000002)
            self.dp.write_ap(0x02000004, 0xe00e1004)
            self.dp.write_ap(0x0200000c, value)
        else:
            original_write(self, address, value)
    MinimalMemAP.read32 = read
    MinimalMemAP.write32 = write
    MinimalMemAP._fk_ap2_access = True


def prepare_debug_domains(uid=None):
    """只修改已核实 H743 的易失 DBGMCU_CR，避免休眠域导致 AP0 读回零。"""
    from h7tool_compat import disable_transfer_block
    disable_transfer_block()
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.session import Session
    from pyocd.probe.debug_probe import DebugProbe
    from pyocd.probe.swj import SWJSequenceSender
    install_dbg_ap2_access()
    bootstrap = Session(None, options={'cmsis_dap.prefer_v1': True,
                                       'cmsis_dap.deferred_transfers': False})
    probes = ConnectHelper.get_all_connected_probes(unique_id=uid)
    if len(probes) != 1:
        raise RuntimeError('必须连接唯一的 H7-TOOL 探针或指定 UID')
    probe = probes[0]
    probe.session = bootstrap
    probe.open()
    try:
        probe.connect(DebugProbe.Protocol.SWD)
        probe.set_clock(100000)
        SWJSequenceSender(probe, False).select_protocol(DebugProbe.Protocol.SWD)
        if not probe.read_dp(0):
            raise RuntimeError('SWD DPIDR 无效')
        probe.assert_reset(False)
        probe.write_dp(0, 0x1e)
        probe.write_dp(4, 0x50000000)
        probe.flush()
        for _ in range(20):
            if probe.read_dp(4) & 0xf0000000 == 0xf0000000:
                break
            time.sleep(0.01)
        else:
            raise RuntimeError('调试电源域未就绪')
        probe.write_dp(8, 0x020000f0)
        probe.flush()
        ap2 = probe.read_ap(12)
        if ap2 & 0x0fffe00f != 0x04770002:
            raise RuntimeError(f'非预期 AP2 标识 {ap2:#x}')
        probe.write_dp(8, 0x02000000)
        probe.flush()
        probe.write_ap(0, 0x03000002)
        probe.flush()

        def read_word(address):
            """从当前选中的 MEM-AP 读取一个寄存器。"""
            probe.write_ap(4, address)
            probe.flush()
            probe.read_ap(4)
            return probe.read_ap(12)

        device = read_word(0xe00e1000)
        if device & 0xfff != 0x450:
            raise RuntimeError(f'目标不是预期 STM32H743：{device:#x}')
        previous = read_word(0xe00e1004)
        probe.write_ap(4, 0xe00e1004)
        probe.flush()
        probe.write_ap(12, previous | 0x70003f)
        probe.flush()
        read_word(0xe00e1004)
        time.sleep(0.1)
        probe.write_dp(8, 0)
        probe.flush()
        probe.write_ap(0, 0x23000052)
        probe.flush()
        cpu = read_word(0xe000ed00)
        aliased = read_word(0x5c001000)
        capacity = read_word(0x1ff1e880) & 0xffff
        if (cpu >> 4) & 0xfff != 0xc27 or capacity != 2048:
            raise RuntimeError(f'AP0 尚不可用：CPUID={cpu:#x} IDC={aliased:#x} Flash={capacity} KiB')
        return dict(deviceId=hex(device), cpuId=hex(cpu), flashKiB=capacity,
                    previousDebugControl=hex(previous), earlyAp0DeviceAlias=hex(aliased))
    finally:
        probe.disconnect()
        probe.close()

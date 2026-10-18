"""Process-local workaround for H7TOOL V2.33 CMSIS-DAP TransferBlock reads.

Use ordinary DAP_Transfer reads and individual AP writes. Disable block
packing at command creation so packet capacity calculations remain correct.
This does not alter installed pyOCD or change target reset/power state.
"""
def disable_transfer_block():
    from pyocd.probe.pydapaccess import dap_access_cmsis_dap as dap
    if getattr(dap._Command, '_h7tool_single_transfer', False):
        return
    original = dap._Command.__init__
    def initialize(self, size):
        original(self, size)
        self._block_allowed = False
    dap._Command.__init__ = initialize
    dap._Command._h7tool_single_transfer = True

    from pyocd.probe.cmsis_dap_probe import CMSISDAPProbe
    def write_words(self, addr, values):
        for value in values:
            self.write_ap(addr, value)
            self.flush()
    CMSISDAPProbe.write_ap_multiple = write_words

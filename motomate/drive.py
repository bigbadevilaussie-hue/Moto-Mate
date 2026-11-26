"""Archive actions gated until OSA owns Moto-Mate delivery."""
def _pending(*args, **kwargs):
    raise RuntimeError("Moto-Mate archive integration pending")
fetch_drive_list = sync_from_drive = backup_firmware_to_drive = _pending

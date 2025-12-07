from enum import IntEnum
import os
import threading

from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog
from openpilot.selfdrive.ui.lib.api_poller import ApiPoller



class PrimeType(IntEnum):
  UNKNOWN = -2
  UNPAIRED = -1
  NONE = 0
  MAGENTA = 1
  LITE = 2
  BLUE = 3
  MAGENTA_NEW = 4
  PURPLE = 5


class PrimeState(ApiPoller):
  FETCH_INTERVAL = 5.0  # seconds between API calls
  API_TIMEOUT = 10.0  # seconds for API requests
  SLEEP_INTERVAL = 0.5  # seconds to sleep between checks in the worker thread

  def __init__(self):
    super().__init__()
    self._params = Params()
    self._lock = threading.Lock()
    self.prime_type: PrimeType = self._load_initial_state()

  def _load_initial_state(self) -> PrimeType:
    prime_type_str = os.getenv("PRIME_TYPE") or self._params.get("PrimeType")
    try:
      if prime_type_str is not None:
        return PrimeType(int(prime_type_str))
    except (ValueError, TypeError):
      pass
    return PrimeType.UNKNOWN

  def _get_url(self):
    return f"v1.1/devices/{self._dongle_id}"

  def _process_api_data(self, data) -> None:
    is_paired = data.get("is_paired", False)
    prime_type = data.get("prime_type", 0)
    self.set_type(PrimeType(prime_type) if is_paired else PrimeType.UNPAIRED)

  def set_type(self, prime_type: PrimeType) -> None:
    with self._lock:
      if prime_type != self.prime_type:
        self.prime_type = prime_type

    self._params.put("PrimeType", int(prime_type))
    cloudlog.info(f"Prime type updated to {prime_type}")

  def get_type(self) -> PrimeType:
    with self._lock:
      return self.prime_type

  def is_prime(self) -> bool:
    with self._lock:
      return bool(self.prime_type > PrimeType.NONE)

  def is_paired(self) -> bool:
    with self._lock:
      return self.prime_type > PrimeType.UNPAIRED

import threading
from abc import ABC, abstractmethod
from openpilot.common.api import api_get
from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog
from openpilot.system.athena.registration import UNREGISTERED_DONGLE_ID
from openpilot.selfdrive.ui.lib.api_helpers import get_token


class ApiPoller(ABC):
  FETCH_INTERVAL: float = 5.0  # How often to attempt a full fetch cycle (seconds)
  API_TIMEOUT: float = 10.0  # Timeout for the API request (seconds)

  def __init__(self):
    self._params = Params()
    self._dongle_id: str | None = None
    self._stop_event = threading.Event()
    self._thread = threading.Thread(target=self._loop, daemon=True)
    self._thread.start()

  @abstractmethod
  def _get_url(self) -> str:
    """Returns the API endpoint URL."""

  @abstractmethod
  def _process_api_data(self, data: dict) -> None:
    """Process the data returned from the API call."""

  def _loop(self) -> None:
    from openpilot.selfdrive.ui.ui_state import ui_state, device

    while not self._stop_event.is_set():
      if not ui_state.started and device._awake:
        self._safe_fetch()

      self._stop_event.wait(self.FETCH_INTERVAL)

  def _safe_fetch(self) -> None:
    try:
      self._dongle_id = self._params.get("DongleId")
      if not self._dongle_id or self._dongle_id == UNREGISTERED_DONGLE_ID:
        return

      identity_token = get_token(self._dongle_id)
      response = api_get(self._get_url(), timeout=self.API_TIMEOUT, access_token=identity_token)
      if response.status_code == 200:
        self._process_api_data(response.json())
    except Exception as e:
      cloudlog.error(f"Error during API data fetch: {e}")


  def stop(self) -> None:
    self._stop_event.set()
    self._thread.join()

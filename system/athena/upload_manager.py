import hashlib
import json
import os
import time
import threading
from collections import deque
from dataclasses import asdict, dataclass
from openpilot.common.params import Params
from openpilot.system.hardware.hw import Paths
from openpilot.common.swaglog import cloudlog


MAX_RETRY_COUNT = 30  # Try for at most 5 minutes if upload fails immediately

def strip_zst_extension(fn: str) -> str:
  return fn[:-4] if fn.endswith('.zst') else fn


@dataclass
class UploadFile:
  fn: str
  url: str
  headers: dict[str, str]
  allow_cellular: bool

  @classmethod
  def from_dict(cls, d: dict):
    return cls(d.get("fn", ""), d.get("url", ""), d.get("headers", {}), d.get("allow_cellular", False))


@dataclass
class UploadItem:
  path: str
  url: str
  headers: dict[str, str]
  created_at: int
  id: str | None
  retry_count: int = 0
  current: bool = False
  progress: float = 0
  allow_cellular: bool = False

  @classmethod
  def from_dict(cls, d: dict):
    return cls(d["path"], d["url"], d["headers"], d["created_at"], d["id"], d["retry_count"], d["current"],
               d["progress"], d["allow_cellular"])


class UploadManager:
  def __init__(self):
    self.upload_lock = threading.Lock()
    self.upload_queue: deque[UploadItem] = deque()
    self.cur_upload_items: dict[int, UploadItem | None] = {}
    self.params = Params()

    self._init_from_cache()

  def get_next(self, tid: int) -> UploadItem | None:
    with self.upload_lock:
      item = self.upload_queue.popleft() if self.upload_queue else None
      self.cur_upload_items[tid] = item
      if item:
        item.current = True
      return item

  def update_cancels(self, cancel_ids: set[str]) -> bool:
    with self.upload_lock:
      initial_size = len(self.upload_queue)
      self.upload_queue = deque(item for item in self.upload_queue if item.id not in cancel_ids)
      return len(self.upload_queue) == initial_size


  def enqueue(self, file: UploadFile) -> tuple[UploadItem | None, bool]:
    path = os.path.join(Paths.log_root(), file.fn)
    if not os.path.exists(path) and not os.path.exists(strip_zst_extension(path)):
      return None, False

    # Skip item if already in queue
    url = file.url.split('?')[0]
    if any(url == item['url'].split('?')[0] for item in self.get_items() if isinstance(item["url"], str)):
      return None, True

    item = UploadItem(
      path=path,
      url=file.url,
      headers=file.headers,
      created_at=int(time.time() * 1000),
      id=None,
      allow_cellular=file.allow_cellular,
    )
    item.id = hashlib.sha1(str(item).encode()).hexdigest()

    self.upload_queue.append(item)
    self.cache()
    return item, False

  def clear_current_uploads(self):
    with self.upload_lock:
      self.cur_upload_items.clear()

  def get_items(self) -> list[UploadItemDict]:
    with self.upload_lock:
      return [asdict(i) for i in list(self.upload_queue) + list(self.cur_upload_items.values())]


  def retry_upload(self, tid: int, end_event, increase_count: bool = True) -> None:
    with self.upload_lock:
      item = self.cur_upload_items[tid]
      if item.retry_count < MAX_RETRY_COUNT:
        if increase_count:
          item.retry_count += 1

        item.progress = 0.0
        item.current = False
        self.upload_queue.append(item)

      self.cur_upload_items[tid] = None
      self.cache()

    for _ in range(10):
      time.sleep(1)
      if end_event.is_set():
        break


  def _init_from_cache(self) -> None:
    try:
      upload_queue_json = self.params.get("AthenadUploadQueue")
      if upload_queue_json:
        for item in json.loads(upload_queue_json):
          self.upload_queue.append(UploadItem.from_dict(item))
    except Exception:
      cloudlog.exception("athena.UploadQueueCache.initialize.exception")

  def cache(self) -> None:
    try:
      items = [asdict(i) for i in self.upload_queue]
      self.params.put("AthenadUploadQueue", json.dumps(items))
    except Exception:
      cloudlog.exception("athena.UploadQueueCache.cache.exception")

  def update_progress(self, tid: int, cur_size: int, total_size: int) -> None:
    with self.upload_lock:
      if item := self.cur_upload_items.get(tid):
        item.progress = cur_size / total_size if cur_size else 1

import timeit
from tools.lib.logreader import LogReader
import cereal.messaging as messaging
from selfdrive.test.openpilotci import get_url
import time

def main():
  # dat = messaging.new_message('controlsState')
  # dat.valid = True
  # controlsState = dat.controlsState
  # controlsState.alertText1 = 'ttt'
  cnt = 10000
  segment = '0375fdf7b1ce594d|2019-06-13--08-32-25--3'
  r, n = segment.rsplit("--", 1)
  lr = LogReader(get_url(r, n))
  e = 0
  d = {'steeringPressed':1}
  for msg in lr:
    if (msg.which() == 'carState'):
      e+=1
      if (e < 20):
        continue
      cs = msg.carState
      print(cs)
      reader_wrapper = messaging.CapnpReaderWrapper(cs)
      t1 = time.time()
      for i in range(cnt):
        a = cs.steeringPressed#cs.cruiseState.speed
        # print(type(cs.cruiseState))
      t2 = time.time()
      # print(cnt)
      for i in range(cnt):
        a = reader_wrapper.steeringPressed
      t3 = time.time()
      for i in range(cnt):
        a = d.get('steeringPressed',None)
      t4 = time.time();
      print('child avg:', 'pycapnp', ((t2 - t1) * 1000), 'readerWraper:', ((t3 - t2) * 1000), 'dict:', ((t4-t3)*1000) )
      break;


if __name__ == "__main__":
  main()

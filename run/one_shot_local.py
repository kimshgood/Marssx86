#!/bin/env python

import os
import sys
import errno
import threading
import string

from time import gmtime, strftime

import defn

def get_time():
  return "["+strftime("%Y-%m-%d %H:%M:%S", gmtime())+"]"

class ThreadClass(threading.Thread):
  def __init__(self, workload, cycles, config):
    threading.Thread.__init__(self)
    self.workload = workload
    self.cycles = cycles
    self.config = config

  def run(self):
    out_dir = "/tmp/vteori/results/"+self.workload+"_"+self.config
    cfg_dir = os.getcwd()+"/cfgs/"
    cfg_name = self.workload+"_"+self.cycles+".cfg"

    command = ("util/run_bench.py -d "+out_dir+" -c "+cfg_dir+cfg_name+" "+self.config)
    os.system(command)

### __main___ ###
try:
  os.chdir('../')
  workload = sys.argv[1]
  cycles = sys.argv[2]

  print get_time()+" "+workload+" "+cycles+" starts!"
  
  ### run base producing a trace ###
  threads = []
  base_t = ThreadClass(workload, cycles, "base")
  base_t.start()
  threads.append(base_t)

  ### run perfect configurations ###
  configs = defn.get_configs(0)
  for config in configs:
    t = ThreadClass(workload, cycles, "perf-"+config)
    t.start()
    threads.append(t)

  for t in threads:
    t.join()

  print get_time()+" "+workload+" "+cycles+"'s 1st round done!"

  ## run perfect configuration combinations ###
  for i in range(1, defn.conf_options):
    threads = []
    configs = defn.get_configs(i)
    for config in configs:
      t = ThreadClass(workload, cycles, "perf-"+string.join(config,'-'))
      t.start()
      threads.append(t)

    for t in threads:
      t.join()

    print get_time()+" "+workload+" "+cycles+"'s "+str(i+1)+"th round  done!"

except IndexError:
  print "Incorrect arguments..."
  print "Usage: ./one_shot_local.py [workload] [cycles]"
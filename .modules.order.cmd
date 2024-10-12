cmd_/media/sf_work/ipc/modules.order := {   echo /media/sf_work/ipc/ipc.ko; :; } | awk '!x[$$0]++' - > /media/sf_work/ipc/modules.order

cmd_/media/sf_work/ipc/Module.symvers := sed 's/\.ko$$/\.o/' /media/sf_work/ipc/modules.order | scripts/mod/modpost -m -a  -o /media/sf_work/ipc/Module.symvers -e -i Module.symvers   -T -

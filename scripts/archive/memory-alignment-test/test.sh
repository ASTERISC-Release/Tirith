rm -f /tmp/gramine_vhostfs_10*

cd ./../gramine-tdx
export PYTHONPATH=$PWD/built-debug/lib/python3.13/site-packages/
cd CI-Examples/align_test
make
gramine-vm align_test

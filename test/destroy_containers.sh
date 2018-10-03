sudo lxc-ls -f
sudo lxc-stop -n test_sleepy_conn_05_gateway
sudo lxc-stop -n test_sleepy_conn_05_relay
sudo lxc-stop -n test_sleepy_conn_05_sleepy
sudo lxc-destroy -n test_sleepy_conn_05_gateway -s
sudo lxc-destroy -n test_sleepy_conn_05_relay -s
sudo lxc-destroy -n test_sleepy_conn_05_sleepy -s
make check
sudo lxc-ls -f

#! /bin/sh
#
# incron       This init.d script is used to start incron
#

### BEGIN INIT INFO
# Provides:          incron
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start, stop or reload incron
# Description:       incron is a file system events scheduler
### END INIT INFO

PATH=/sbin:/bin:/usr/sbin:/usr/bin
INCROND=/usr/sbin/incrond
NAME=incron
DESC="File system events scheduler"
INCROND_PID=/var/run/incrond.pid
INCROND_CONF=/etc/incron.conf

if [ ! -x "$INCROND" ]; then
    exit 0
fi

# loading lsb functions
. /lib/lsb/init-functions

case "$1" in
    start)
        log_daemon_msg "Starting $DESC"
        log_progress_msg "$NAME"
        start_daemon -p "$INCROND_PID" "$INCROND" -f "$INCROND_CONF"
        log_end_msg $?
        ;;
    stop)
        log_daemon_msg "Stopping $DESC"
        log_progress_msg "$NAME"
        killproc -p "$INCROND_PID" "$INCROND"
        log_end_msg $?
        ;;
    reload)
        log_daemon_msg "Reloading $DESC"
        log_progress_msg "$NAME"
        echo -n
        log_end_msg $?
        ;;
    restart|force-reload)
        $0 stop
        sleep 1
        $0 start
        ;;
    status)
        if ! status_of_proc "$(basename "$INCROND")" "$NAME" ; then
            exit $?
        fi
        ;;
    *)
        N=/etc/init.d/$NAME
        echo "Usage: $N {start|stop|restart|reload|force-reload|status}" >&2
        exit 1
        ;;
esac

exit 0

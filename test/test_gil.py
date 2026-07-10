import threading
import time

from reflect_bind.reflect_bind import Queue

q = Queue()


def background_worker(stop_event, counter):
    """后台线程：持续增加计数器，模拟其他 Python 任务"""
    print("Background worker started.")
    while not stop_event.is_set():
        counter[0] += 1
        # 稍微休息一下，避免占用过多 CPU，但这不影响 GIL 的测试逻辑
        # time.sleep(0.0001)
    print(f"Background worker stopped. Counter: {counter[0]}")


def test_gil_release(wait_time):
    """测试 C 扩展函数是否释放了 GIL"""
    stop_event = threading.Event()
    counter = [0]

    # 启动后台线程
    worker_thread = threading.Thread(
        target=background_worker, args=(stop_event, counter)
    )
    worker_thread.start()

    # 给后台线程一点启动时间
    # time.sleep(0.1)

    # 记录初始计数
    initial_count = counter[0]
    print(f"Initial counter: {initial_count}")

    # 调用 C 扩展函数 (在这里指定等待时间)
    print(f"Calling C function with wait_time={wait_time}...")
    # 替换为你要测试的函数

    q.pop(wait_time)
    # testmodule.test_hold_gil(wait_time)  # 用于对比

    # 记录结束计数
    final_count = counter[0]
    print(f"Final counter: {final_count}")
    print(f"Counter increased by: {final_count - initial_count}")

    # 停止后台线程
    stop_event.set()
    worker_thread.join()

    # 判断结果
    if final_count - initial_count > 1000:  # 设定一个阈值
        print("✅ Result: GIL was RELEASED. Background thread ran significantly.")
    else:
        print(
            "❌ Result: GIL was NOT released (or was held for too long). Background thread was blocked."
        )


if __name__ == "__main__":
    # 指定等待时间 (秒)
    WAIT_TIME = 3000
    test_gil_release(WAIT_TIME)

/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#include <maxscale/watchedworker.hh>
#include <maxscale/mainworker.hh>

namespace maxscale
{

class WatchedWorker::WatchdogNotifier
{
    WatchdogNotifier(const WatchdogNotifier&) = delete;
    WatchdogNotifier& operator=(const WatchdogNotifier&) = delete;

public:
    WatchdogNotifier(mxs::WatchedWorker* pOwner)
        : m_owner(*pOwner)
        , m_nClients(0)
        , m_terminate(false)
    {
        m_thread = std::thread([this] {
                                   uint32_t interval = mxs::MainWorker::watchdog_interval().secs();
                                   timespec timeout = {interval, 0};

                                   while (!mxb::atomic::load(&m_terminate, mxb::atomic::RELAXED))
                                   {
                                        // We will wakeup when someone wants the notifier to run,
                                        // or when MaxScale is going down.
                                       m_sem_start.wait();

                                       if (!mxb::atomic::load(&m_terminate, mxb::atomic::RELAXED))
                                       {
                                            // If MaxScale is not going down...
                                           do
                                           {
                                                // we ensure the worker appears to be ticking
                                               m_owner.mark_ticking_if_currently_not();
                                           }
                                           while (!m_sem_stop.timedwait(timeout));
                                            // until the semaphore is actually posted, which it will be
                                            // once the notification should stop.
                                       }
                                   }
                               });
    }

    ~WatchdogNotifier()
    {
        mxb_assert(m_nClients == 0);
        mxb::atomic::store(&m_terminate, true, mxb::atomic::RELAXED);
        m_sem_start.post();
        m_thread.join();
    }

    void start()
    {
        Guard guard(m_lock);
        mxb::atomic::add(&m_nClients, 1, mxb::atomic::RELAXED);

        if (m_nClients == 1)
        {
            m_sem_start.post();
        }
    }

    void stop()
    {
        Guard guard(m_lock);
        mxb::atomic::add(&m_nClients, -1, mxb::atomic::RELAXED);
        mxb_assert(m_nClients >= 0);

        if (m_nClients == 0)
        {
            m_sem_stop.post();
        }
    }

private:
    using Guard = std::lock_guard<std::mutex>;

    mxs::WatchedWorker& m_owner;
    int                 m_nClients;
    bool                m_terminate;
    std::thread         m_thread;
    std::mutex          m_lock;
    mxb::Semaphore      m_sem_start;
    mxb::Semaphore      m_sem_stop;
};


WatchedWorker::WatchedWorker(MainWorker* pMain)
    : m_main(*pMain)
    , m_ticking(true)
{
    if (MainWorker::watchdog_interval().count() != 0)
    {
        m_pWatchdog_notifier = new WatchdogNotifier(this);
    }

    m_main.add(this);
}

WatchedWorker::~WatchedWorker()
{
    m_main.remove(this);

    delete m_pWatchdog_notifier;
}

void WatchedWorker::start_watchdog_workaround()
{
    if (m_pWatchdog_notifier)
    {
        m_pWatchdog_notifier->start();
    }
}

void WatchedWorker::stop_watchdog_workaround()
{
    if (m_pWatchdog_notifier)
    {
        m_pWatchdog_notifier->stop();
    }
}

void WatchedWorker::epoll_tick()
{
    mark_ticking_if_currently_not();

    epoll_tock();
}

}
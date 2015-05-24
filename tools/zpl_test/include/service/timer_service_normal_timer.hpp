#ifndef TIMER_SERVICE_NORMAL_TIMER_HPP_INCLUDED
#define TIMER_SERVICE_NORMAL_TIMER_HPP_INCLUDED

#include <chrono>
#include <map>
#include <mutex>
#include <list>
#include <condition_variable>
#include <transport/app_message.h>
#include <service/timer_service.h>
#include <boost/bind.hpp>

using namespace std;
using zhicloud::service::TimerService;
using zhicloud::transport::AppMessage;

class TimerServiceNormalTimer{
        typedef int16_t timer_id_type;
    public:
        TimerServiceNormalTimer():_result(false)
        {
        }
        ~TimerServiceNormalTimer(){
        }
        bool test(){
            list< uint32_t > timeout_list = {2, 4, 5, 8};
            _timer_service.bindHandler(boost::bind(&TimerServiceNormalTimer::onTimeoutEvent, this, _1));
            _timer_service.start();
            {
                uint32_t session_id(0);
                lock_type lock(_mutex);
                for(const uint32_t& timeout:timeout_list){
                    session_id++;
                    timer_id_type timer_id = _timer_service.setTimer(timeout, session_id);
                    _timer_map.emplace(timer_id, chrono::high_resolution_clock::now() );
                    _timeout_map.emplace(timer_id, timeout);
                    BOOST_LOG_TRIVIAL(info) << boost::format("[%08X]timer %d set, timeout %d second(s)") % session_id %timer_id %timeout;
                }
            }
            timeout_list.sort();
            uint32_t max_timeout = timeout_list.back() + 2;
            {
                lock_type lock(_mutex);
                _finish_event.wait_for(lock, chrono::seconds(max_timeout));
            }
            _timer_service.stop();

            if(_timeout_map.empty()){
                _result = true;
            }
            return _result;
        }
    private:
        void onTimeoutEvent(list < AppMessage >& event_list){
            for(const AppMessage& msg:event_list){
                BOOST_LOG_TRIVIAL(info) << boost::format("[%08X]timer %d invoked") %msg.session %msg.sequence;
                timer_id_type timer_id = msg.sequence;
                lock_type lock(_mutex);
                map< timer_id_type, chrono::time_point< chrono::high_resolution_clock > >::const_iterator timepoint_ir = _timer_map.find(timer_id);
                if(_timer_map.end() == timepoint_ir){
                    BOOST_LOG_TRIVIAL(error) << boost::format("invalid timer %d") %timer_id;
                    return;
                }
                uint32_t elapsed = chrono::duration_cast< chrono::seconds >( chrono::high_resolution_clock::now() - timepoint_ir->second).count();
                map< timer_id_type, uint32_t >::iterator timeout_ir = _timeout_map.find(timer_id);
                if(_timeout_map.end() == timeout_ir){
                    BOOST_LOG_TRIVIAL(error) << boost::format("can't get timeout value for timer %d") %timer_id;
                    return;
                }
                if(timeout_ir->second != elapsed){
                    BOOST_LOG_TRIVIAL(error) << boost::format("timeout invoked in unexpect time, elapsed %d, expect %d") %elapsed %timeout_ir->second;
                    return;
                }
                else{
                    _timeout_map.erase(timeout_ir);
                    if(_timeout_map.empty())
                        _finish_event.notify_all();
                }
            }
        }
    private:
        map< timer_id_type, chrono::time_point< chrono::high_resolution_clock > > _timer_map;
        map< timer_id_type, uint32_t > _timeout_map;
        TimerService _timer_service;
        mutex _mutex;
        bool _result;
        condition_variable _finish_event;
        typedef unique_lock< mutex > lock_type;

};

#endif // TIMER_SERVICE_NORMAL_TIMER_HPP_INCLUDED

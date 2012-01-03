/* Copyright 2011 ZAO "Begun".
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 *
 * Гарантируется работа с MySQL5.1.23
 *
 */

#ifndef __SLAVE_SLAVE_H_
#define __SLAVE_SLAVE_H_


#include <string>
#include <vector>
#include <map>
#include <set>

#include <mysql/mysql.h>

#include "slave_log_event.h"
#include "SlaveStats.h"


namespace slave
{

struct MasterInfo {

    unsigned int port;
    std::string host;
    std::string user;
    std::string password;
    std::string master_log_name;
    unsigned long master_log_pos;
    unsigned int connect_retry;

    std::string master_info_file;

    MasterInfo() : port(3306), master_log_pos(0), connect_retry(10) {}
};


class Slave
{
public:

    typedef std::vector<std::pair<std::string, std::string> > table_order_t;
    typedef std::map<std::pair<std::string, std::string>, callback> callbacks_t;


private:
    static inline bool falseFunction() { return false; };

    MYSQL mysql;

    int m_server_id;

    MasterInfo m_master_info;

    table_order_t m_table_order;
    callbacks_t m_callbacks;

    typedef boost::function<void (unsigned int)> xid_callback_t;
    xid_callback_t m_xid_callback;

    RelayLogInfo m_rli;


    void createDatabaseStructure_(table_order_t& tabs, RelayLogInfo& rli) const;

public:

    Slave() {}

    Slave(const MasterInfo& _master_info) : m_master_info(_master_info) {}

    void setMasterInfo(const MasterInfo& aMasterInfo) { m_master_info = aMasterInfo; }
    const MasterInfo& masterInfo() const { return m_master_info; }

    typedef std::pair<std::string, unsigned int> binlog_pos_t;
    binlog_pos_t getLastBinlog();

    void setCallback(const std::string& _db_name, const std::string& _tbl_name, callback _callback)
    {
        m_table_order.push_back(std::make_pair(_db_name, _tbl_name));
        m_callbacks[std::make_pair(_db_name, _tbl_name)] = _callback;

        stats::initTableCount(_db_name + "." + _tbl_name);
    }

    void setXidCallback(xid_callback_t _callback)
    {
        m_xid_callback = _callback;
    }

    void get_remote_binlog( const boost::function< bool() >& _interruptFlag = &Slave::falseFunction );

    void createDatabaseStructure() {

        m_rli.clear();

        createDatabaseStructure_(m_table_order, m_rli);

        for (RelayLogInfo::name_to_table_t::iterator i = m_rli.m_table_map.begin(); i != m_rli.m_table_map.end(); ++i) {
            i->second->m_callback = m_callbacks[i->first];
        }
    }

    RelayLogInfo getRli() const {
        return m_rli;
    }

    table_order_t getTableOrder() const {
        return m_table_order;
    }

    void init();

    int serverId() const { return m_server_id; }

    // Closes connection, opened in get_remotee_binlog. Should be called if your have get_remote_binlog
    // blocked on reading data from mysql server in the separate thread and you want to stop this thread.
    // You should take care that interruptFlag will return 'true' after connection is closed.
    void close_connection();

protected:


    void check_master_version();

    void check_master_binlog_format();

    int process_event(const slave::Basic_event_info& bei, RelayLogInfo &rli, unsigned long long pos);

    void request_dump(const std::string& logname, unsigned long start_position, MYSQL* mysql);

    ulong read_event(MYSQL* mysql);

    std::map<std::string,std::string> getRowType(const std::string& db_name,
                                                 const std::set<std::string>& tbl_names) const;

    void createTable(RelayLogInfo& rli,
                     const std::string& db_name, const std::string& tbl_name,
                     const collate_map_t& collate_map, nanomysql::Connection& conn) const;

    void register_slave_on_master(MYSQL* mysql);
    void deregister_slave_on_master(MYSQL* mysql);

    void generateSlaveId();

};

}// slave

#endif

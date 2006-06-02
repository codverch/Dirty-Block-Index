/*
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Nathan Binkert
 */

#ifndef __BASE_STATS_MYSQL_HH__
#define __BASE_STATS_MYSQL_HH__

#include <map>
#include <string>

#include "base/stats/output.hh"

namespace MySQL { class Connection; }
namespace Stats {

class MainBin;
class DistDataData;
class MySqlRun;
bool MySqlConnected();
extern MySqlRun MySqlDB;

struct SetupStat
{
    std::string name;
    std::string descr;
    std::string type;
    bool print;
    uint16_t prereq;
    int8_t prec;
    bool nozero;
    bool nonan;
    bool total;
    bool pdf;
    bool cdf;
    double min;
    double max;
    double bktsize;
    uint16_t size;

    void init();
    unsigned setup();
};

class InsertData
{
  private:
    char *query;
    int size;
    bool first;
    static const int maxsize = 1024*1024;

  public:
    MySqlRun *run;

  public:
    uint64_t tick;
    double data;
    uint16_t stat;
    uint16_t bin;
    int16_t x;
    int16_t y;

  public:
    InsertData();
    ~InsertData();

    void flush();
    void insert();
};

class MySql : public Output
{
  protected:
    SetupStat stat;
    InsertData newdata;
    std::list<FormulaData *> formulas;
    bool configured;

  protected:
    std::map<int, int> idmap;

    void insert(int sim_id, int db_id)
    {
        using namespace std;
        idmap.insert(make_pair(sim_id, db_id));
    }

    int find(int sim_id)
    {
        using namespace std;
        map<int,int>::const_iterator i = idmap.find(sim_id);
        assert(i != idmap.end());
        return (*i).second;
    }
  public:
    // Implement Visit
    virtual void visit(const ScalarData &data);
    virtual void visit(const VectorData &data);
    virtual void visit(const DistData &data);
    virtual void visit(const VectorDistData &data);
    virtual void visit(const Vector2dData &data);
    virtual void visit(const FormulaData &data);

    // Implement Output
    virtual bool valid() const;
    virtual void output();

  protected:
    // Output helper
    void output(MainBin *bin);
    void output(const DistDataData &data);
    void output(const ScalarData &data);
    void output(const VectorData &data);
    void output(const DistData &data);
    void output(const VectorDistData &data);
    void output(const Vector2dData &data);
    void output(const FormulaData &data);

    void configure();
    bool configure(const StatData &data, std::string type);
    void configure(const ScalarData &data);
    void configure(const VectorData &data);
    void configure(const DistData &data);
    void configure(const VectorDistData &data);
    void configure(const Vector2dData &data);
    void configure(const FormulaData &data);
};

/* namespace Stats */ }

#endif // __BASE_STATS_MYSQL_HH__

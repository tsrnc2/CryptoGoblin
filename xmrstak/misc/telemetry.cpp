/*
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  *
  * Additional permission under GNU GPL version 3 section 7
  *
  * If you modify this Program, or any covered work, by linking or combining
  * it with OpenSSL (or a modified version of that library), containing parts
  * covered by the terms of OpenSSL License and SSLeay License, the licensors
  * of this Program grant you additional permission to convey the resulting work.
  *
  */

#include "telemetry.hpp"
#include "xmrstak/net/msgstruct.hpp"

#include <cmath>
#include <cstring>
#include <chrono>

namespace xmrstak
{

telemetry::telemetry(size_t iThd)
{
    ppHashCounts = new uint32_t*[iThd];
    ppTimestamps = new uint32_t*[iThd];
    iBucketTop = new uint32_t[iThd];
    mtx = new std::mutex[iThd];

    for (size_t i = 0; i < iThd; i++)
    {
        ppHashCounts[i] = new uint32_t[iBucketSize];
        ppTimestamps[i] = new uint32_t[iBucketSize];
        iBucketTop[i] = 0;
        memset(ppHashCounts[i], 0, sizeof(uint32_t) * iBucketSize);
        memset(ppTimestamps[i], 0, sizeof(uint32_t) * iBucketSize);
    }
}

double telemetry::calc_telemetry_data(size_t iLastMillisec, size_t iThread)
{
    uint32_t iEarliestHashCnt = 0;
    uint32_t iEarliestStamp = 0;
    uint32_t iLatestStamp = 0;
    uint32_t iLatestHashCnt = 0;
    bool bHaveFullSet = false;

    std::unique_lock<std::mutex> lk(mtx[iThread]);
    uint32_t iTimeNow = (uint32_t)get_timestamp_ms();

    //Start at 1, buckettop points to next empty
    for (size_t i = 1; i < iBucketSize; i++)
    {
        size_t idx = (iBucketTop[iThread] - i) & iBucketMask; //overflow expected here
        uint32_t iTmpStamp = ppTimestamps[iThread][idx];

        if (iTmpStamp == 0)
            break; //That means we don't have the data yet

        uint32_t iTmpHashCnt = ppHashCounts[iThread][idx];

        if (iLatestStamp == 0)
        {
            iLatestStamp = iTmpStamp;
            iLatestHashCnt = iTmpHashCnt;
        }

        if ((iTimeNow - iTmpStamp) > iLastMillisec)
        {
            bHaveFullSet = true;
            break; //We are out of the requested time period
        }

        iEarliestStamp = iTmpStamp;
        iEarliestHashCnt = iTmpHashCnt;
    }
    lk.unlock();

    if (!bHaveFullSet || iEarliestStamp == 0 || iLatestStamp == 0)
        return nan("");

    //Don't think that can happen, but just in case
    if (iLatestStamp - iEarliestStamp == 0)
        return nan("");

    double fHashes, fTime;
    fHashes = static_cast<double>(iLatestHashCnt - iEarliestHashCnt);
    fTime = static_cast<double>(iLatestStamp - iEarliestStamp);
    fTime /= 1000.0;

    return fHashes / fTime;
}

void telemetry::push_perf_value(size_t iThd, uint64_t iHashCount, uint64_t iTimestamp)
{
    std::unique_lock<std::mutex> lk(mtx[iThd]);
    size_t iTop = iBucketTop[iThd];
    ppHashCounts[iThd][iTop] = (uint32_t)iHashCount;
    ppTimestamps[iThd][iTop] = (uint32_t)iTimestamp;

    iBucketTop[iThd] = (iTop + 1) & iBucketMask;
}

} // namespace xmrstak

/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include <MonthlyChallenge.hpp>
#include <Social.hpp>
#include <Achievements.hpp>

#include <crogine/core/SysTime.hpp>

#include <bitset>

constexpr std::array<std::int32_t, 12u> MonthDays =
{
    31,28,31,30,31,30,31,31,30,31,30,31
};

MonthlyChallenge::MonthlyChallenge()
    : m_month   (-1),
    m_day       (0),
    m_leapYear  (false)
{
    //fetch the current month
    auto ts = static_cast<std::time_t>(cro::SysTime::epoch());
    auto td = std::localtime(&ts);

    m_month = td->tm_mon;
    m_day = td->tm_mday;

    auto year = 1900 + td->tm_year;
    m_leapYear = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
}

//public
void MonthlyChallenge::updateChallenge(std::int32_t id, std::int32_t value)
{
    if (m_month != -1 && id == m_month
        && Achievements::getActive())
    {
        //make sure we're not already complete else the completion
        //message will get raised mutliple times...
        auto progress = getProgress();
        if (progress.value != progress.target)
        {

            if (m_challenges[id].type == Challenge::Counter)
            {
                m_challenges[id].value = std::min(m_challenges[id].value + 1, m_challenges[id].targetValue);

                auto* msg = cro::App::postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
                msg->type = Social::SocialEvent::MonthlyProgress;
                msg->level = m_challenges[id].value;
                msg->reason = m_challenges[id].targetValue;
                msg->playerID = id;
            }
            else if (m_challenges[id].type == Challenge::Flag
                && value < 32)
            {
                auto oldVal = m_challenges[id].value;
                m_challenges[id].value |= (1 << value);

                if (oldVal != m_challenges[id].value)
                {
                    std::bitset<sizeof(Challenge::value)> value(m_challenges[id].value);
                    std::bitset<sizeof(Challenge::targetValue)> target(m_challenges[id].targetValue);

                    auto* msg = cro::App::postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
                    msg->type = Social::SocialEvent::MonthlyProgress;
                    msg->level = static_cast<std::int32_t>(value.count());
                    msg->reason = static_cast<std::int32_t>(target.count());
                    msg->playerID = id;
                }
            }

            //TODO write stat to storage
        }
    }
}

void MonthlyChallenge::refresh()
{
    if (m_month > -1 && m_month < ChallengeID::Count)
    {
        for (auto i = 0; i < ChallengeID::Count; ++i)
        {
            if (i == m_month)
            {
                //TODO fetch current value
                //m_challenges[i].value = v;
            }
            else
            {
                //TODO reset stat
            }
        }
    }
}

MonthlyChallenge::Progress MonthlyChallenge::getProgress() const
{
    Progress retVal;
    if (m_month != -1)
    {
        if (m_challenges[m_month].type == Challenge::Counter)
        {
            retVal.target = m_challenges[m_month].targetValue;
            retVal.value = m_challenges[m_month].value;
        }
        else
        {
            std::bitset<sizeof(Challenge::value)> value(m_challenges[m_month].value);
            std::bitset<sizeof(Challenge::targetValue)> target(m_challenges[m_month].targetValue);

            retVal.value = static_cast<std::int32_t>(value.count());
            retVal.target = static_cast<std::int32_t>(target.count());
        }

        retVal.index = m_month;
    }

    return retVal;
}

cro::String MonthlyChallenge::getProgressString() const
{
    cro::String ret;

    if (m_month != -1)
    {
        auto progress = getProgress();
        ret = ChallengeDescriptions[m_month] + "\n";
        if (progress.value != progress.target)
        {
            ret += "Progress: " + std::to_string(progress.value) + "/" + std::to_string(progress.target);

            auto monthDays = MonthDays[m_month - 1];
            if (m_month == 2 && m_leapYear)
            {
                monthDays++;
            }

            const auto remain = monthDays - m_day;
            if (remain == 0)
            {
                ret += "\nFinal Day!";
            }
            else
            {
                ret += "\n" + std::to_string(remain) + " days remaining";
            }
        }
        else
        {
            ret += "Completed!";
        }
    }

    return ret;
}
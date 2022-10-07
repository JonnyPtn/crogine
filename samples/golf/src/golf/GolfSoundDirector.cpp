/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "GolfSoundDirector.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"
#include "ScoreStrings.hpp"
#include "Clubs.hpp"
#include "CommandIDs.hpp"
#include "VatAnimationSystem.hpp"

#include <crogine/audio/AudioResource.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/util/Random.hpp>

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

namespace
{
    constexpr float VoiceDelay = 0.5f;
}

GolfSoundDirector::GolfSoundDirector(cro::AudioResource& ar)
    : m_currentClient   (0),
    m_currentPlayer     (0),
    m_honourID          (0),
    m_newHole           (false)
{
    //this must match with AudioID enum
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/ball/putt01.wav",
        "assets/golf/sound/ball/putt02.wav",
        "assets/golf/sound/ball/putt03.wav",
        
        "assets/golf/sound/ball/swing01.wav",
        "assets/golf/sound/ball/swing02.wav",
        "assets/golf/sound/ball/swing03.wav",

        "assets/golf/sound/ball/wedge01.wav",

        "assets/golf/sound/ball/holed.wav",
        "assets/golf/sound/ball/near_holed.wav",
        "assets/golf/sound/ball/near_miss.wav",
        "assets/golf/sound/ball/splash.wav",
        "assets/golf/sound/ball/drop.wav",
        "assets/golf/sound/ball/scrub.wav",
        "assets/golf/sound/ball/stone.wav",

        "assets/golf/sound/holes/albatross.wav",
        "assets/golf/sound/holes/birdie.wav",
        "assets/golf/sound/holes/bogie.wav",
        "assets/golf/sound/holes/bogie_double.wav",
        "assets/golf/sound/holes/bogie_triple.wav",
        "assets/golf/sound/holes/eagle.wav",
        "assets/golf/sound/holes/hole.wav",
        "assets/golf/sound/holes/par.wav",
        "assets/golf/sound/holes/hio.wav",
        "assets/golf/sound/holes/in_one.wav",

        "assets/golf/sound/holes/draw01.wav",
        "assets/golf/sound/holes/draw02.wav",
        "assets/golf/sound/billiards/announcer/win.wav",
        "assets/golf/sound/billiards/announcer/lose.wav",

        "assets/golf/sound/ball/applause.wav",
        "assets/golf/sound/ball/applause_plus.wav",

        "assets/golf/sound/terrain/bunker01.wav",
        "assets/golf/sound/terrain/bunker02.wav",
        "assets/golf/sound/terrain/bunker03.wav",
        "assets/golf/sound/terrain/bunker04.wav",
        "assets/golf/sound/terrain/bunker05.wav",
        "assets/golf/sound/terrain/fairway01.wav",
        "assets/golf/sound/terrain/fairway02.wav",
        "assets/golf/sound/terrain/green01.wav",
        "assets/golf/sound/terrain/green02.wav",
        "assets/golf/sound/terrain/rough01.wav",
        "assets/golf/sound/terrain/rough02.wav",
        "assets/golf/sound/terrain/scrub01.wav",
        "assets/golf/sound/terrain/scrub02.wav",
        "assets/golf/sound/terrain/scrub03.wav",
        "assets/golf/sound/terrain/scrub04.wav",
        "assets/golf/sound/terrain/water01.wav",
        "assets/golf/sound/terrain/water02.wav",
        "assets/golf/sound/terrain/water03.wav",

        "assets/golf/sound/holes/honour.wav",
        "assets/golf/sound/kudos/swing01.wav",
        "assets/golf/sound/kudos/swing02.wav",
        "assets/golf/sound/kudos/swing03.wav",

        "assets/golf/sound/kudos/hook.wav",
        "assets/golf/sound/kudos/slice.wav",

        "assets/golf/sound/kudos/drive_excellent.wav",
        "assets/golf/sound/kudos/drive_good.wav",
        "assets/golf/sound/kudos/drive_poor.wav",

        "assets/golf/sound/kudos/putt02.wav",
        "assets/golf/sound/kudos/putt01.wav",
        "assets/golf/sound/holes/gimme.wav",

        "assets/golf/sound/ambience/burst.wav",
        "assets/golf/sound/holes/airmail.wav",
        "assets/golf/sound/ambience/birds01.wav",
        "assets/golf/sound/ambience/billboard_swing.wav",
        "assets/golf/sound/ambience/billboard_rewind.wav",

        "assets/golf/sound/ambience/foot01.wav",
        "assets/golf/sound/ambience/foot02.wav",
        "assets/golf/sound/ambience/foot03.wav",
        "assets/golf/sound/ambience/foot04.wav",
    };

    std::fill(m_audioSources.begin(), m_audioSources.end(), nullptr);
    for (auto i = 0; i < AudioID::Count; ++i)
    {
        auto id = ar.load(FilePaths[i]); //TODO do we really want to assume none of these are streamed??
        if (id != -1)
        {
            m_audioSources[i] = &ar.get(id);
        }
        else
        {
            //get a default sound so we at least don't have nullptr
            m_audioSources[i] = &ar.get(1010101);
        }
    }

    for (auto& a : m_playerIndices)
    {
        std::fill(a.begin(), a.end(), -1);
    }
}

//public
void GolfSoundDirector::handleMessage(const cro::Message& msg)
{
    static constexpr float MinBallDistance = 100.f; //dist sqr
    static constexpr float MinHoleDistance = 9.f; //dist to hole sqr

    if (cro::AudioMixer::hasAudioRenderer())
    {
        switch (msg.id)
        {
        default: break;
        case Social::MessageID::SocialMessage:
        {
            const auto& data = msg.getData<Social::SocialEvent>();
            if (data.type == Social::SocialEvent::LevelUp)
            {
                playSound(AudioID::ApplausePlus, glm::vec3(0.f)).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
            }
        }
            break;
        case cro::Message::SpriteAnimationMessage:
        {
            const auto& data = msg.getData<cro::Message::SpriteAnimationEvent>();
            if (data.userType == SpriteAnimID::Medal) //scoreboard star animation
            {
                playSound(cro::Util::Random::value(AudioID::Swing01, AudioID::Swing03), glm::vec3(0.f)).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
            }
        }
        break;
        case cro::Message::SkeletalAnimationMessage:
        {
            const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
            if (data.userType == SpriteAnimID::BillboardRewind)
            {
                auto sound = playSound(AudioID::BillboardRewind, data.position, 1.2f);
                sound.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                sound.getComponent<cro::AudioEmitter>().setRolloff(0.65f);
            }
            else if (data.userType == SpriteAnimID::BillboardSwing)
            {
                auto sound = playSound(AudioID::BillboardSwing, data.position, 1.2f);
                sound.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                sound.getComponent<cro::AudioEmitter>().setRolloff(0.65f);
            }
            else if (data.userType == SpriteAnimID::Footstep)
            {
                auto sound = playSound(cro::Util::Random::value(AudioID::Foot01, AudioID::Foot04), data.position, 0.3f);
                sound.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                sound.getComponent<cro::AudioEmitter>().setRolloff(0.74f);
                sound.getComponent<cro::AudioEmitter>().setPitch(1.f + cro::Util::Random::value(-0.2f, 0.2f));
            }
        }
        break;
        case MessageID::GolfMessage:
        {
            const auto& data = msg.getData<GolfEvent>();
            switch (data.type)
            {
            default: break;
            case GolfEvent::RoundEnd:
                if (data.score == 0)
                {
                    playSoundDelayed(AudioID::Win, data.position, 1.6f, 1.f, MixerChannel::Voice);
                }
                else
                {
                    playSoundDelayed(AudioID::Lose, data.position, 1.6f, 1.f, MixerChannel::Voice);
                }
                break;
            case GolfEvent::Gimme:
                //if (cro::Util::Random::value(0, 2) != 0)
            {
                playSoundDelayed(AudioID::Gimme, data.position, 1.6f, 1.f, MixerChannel::Voice);
            }
            break;
            case GolfEvent::BirdHit:
                playSound(AudioID::Birds, data.position, 1.6f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                break;
            case GolfEvent::DroneHit:
                playSound(AudioID::Burst, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                playSoundDelayed(AudioID::Airmail, glm::vec3(0.f), 1.2f, 1.f, MixerChannel::Voice);
                break;
            case GolfEvent::HoleWon:
                if (auto idx = m_playerIndices[data.client][data.player]; idx > -1)
                {
                    static const std::string emitterName = "celebrate";
                    if (m_playerVoices[idx].hasEmitter(emitterName))
                    {
                        playAvatarSound(idx, emitterName, glm::vec3(0.f)).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Voice);
                    }
                }
                break;
            case GolfEvent::HoleDrawn:
                playSoundDelayed(cro::Util::Random::value(AudioID::Draw01, AudioID::Draw02), glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                break;
            case GolfEvent::HoleInOne:
                playSound(AudioID::ScoreHIO, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                break;
            case GolfEvent::DriveComplete:
                if (cro::Util::Random::value(0, 2) != 0)
                {
                    switch (data.score)
                    {
                    default: break;
                    case 1:
                        playSoundDelayed(AudioID::DriveBad, glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                        break;
                    case 2:
                        playSoundDelayed(AudioID::DriveGood, glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                        break;
                    case 3:
                        playSoundDelayed(AudioID::DriveExcellent, glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                        break;
                    }
                }
                break;
            case GolfEvent::ClubChanged:
                if (data.score != 0)
                {
                    playSound(AudioID::Putt01, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
                }
                break;
            case GolfEvent::HookedBall:
                //if (glm::length(data.position) > MinBallDistance)
            {
                playSound(AudioID::Hook, glm::vec3(0.f));
            }
            break;
            case GolfEvent::SlicedBall:
                //if (glm::length(data.position) > MinBallDistance)
            {
                playSound(AudioID::Slice, glm::vec3(0.f));
            }
            break;
            case GolfEvent::NiceShot:
                playSound(cro::Util::Random::value(AudioID::NiceSwing01, AudioID::NiceSwing03), data.position);
                break;
            case GolfEvent::ClubSwing:
            {
                if (data.terrain == TerrainID::Green)
                {
                    playSound(cro::Util::Random::value(AudioID::Putt01, AudioID::Putt03), data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
                else if (data.terrain == TerrainID::Bunker)
                {
                    playSound(AudioID::Wedge, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
                else
                {
                    playSound(cro::Util::Random::value(AudioID::Swing01, AudioID::Swing03), data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
            }
            break;
            case GolfEvent::Scored:
                switch (data.score)
                {
                default:
                    //playSoundDelayed(AudioID::NearMiss, data.position, 0.5f, 0.4f, MixerChannel::Effects);
                    playSoundDelayed(AudioID::ScoreHole, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::HIO:
                    playSoundDelayed(AudioID::ScoreInOne, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Albatross:
                    playSoundDelayed(AudioID::ScoreAlbatross, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Birdie:
                    playSoundDelayed(AudioID::ScoreBirdie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Bogie:
                    playSoundDelayed(AudioID::ScoreBogie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::DoubleBogie:
                    playSoundDelayed(AudioID::ScoreDoubleBogie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Eagle:
                    playSoundDelayed(AudioID::ScoreEagle, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Par:
                    playSoundDelayed(AudioID::ScorePar, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::TripleBogie:
                    playSoundDelayed(AudioID::ScoreTripleBogie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                }

                if (data.score <= ScoreID::Par)
                {
                    playSoundDelayed(AudioID::Applause, glm::vec3(0.f), 0.8f, MixerChannel::Effects);
                    applaud();
                }

                if (data.travelDistance > 25.f) //5m
                {
                    playSoundDelayed(AudioID::ApplausePlus, glm::vec3(0.f), 1.2f, MixerChannel::Effects);
                    applaud();

                    //sunk an extra long putt
                    if (data.club == ClubID::Putter)
                    {
                        playSoundDelayed(cro::Util::Random::value(AudioID::NicePutt01, AudioID::NicePutt02), glm::vec3(0.f), 2.2f, MixerChannel::Voice);
                        Achievements::incrementStat(StatStrings[StatID::LongPutts]);
                    }
                }

                break;
            case GolfEvent::BallLanded:
                if (data.travelDistance > MinBallDistance)
                {
                    switch (data.terrain)
                    {
                    default: break;
                    case TerrainID::Bunker:
                        playSound(cro::Util::Random::value(AudioID::TerrainBunker01, AudioID::TerrainBunker05), glm::vec3(0.f));

                        if (auto idx = m_playerIndices[m_currentClient][m_currentPlayer]; idx > -1)
                        {
                            playAvatarSoundDelayed(idx, "scrub", glm::vec3(0.f), 2.f);
                        }

                        break;
                    case TerrainID::Fairway:
                        playSound(cro::Util::Random::value(AudioID::TerrainFairway01, AudioID::TerrainFairway02), glm::vec3(0.f));
                        break;
                    case TerrainID::Scrub:
                        playSound(cro::Util::Random::value(AudioID::TerrainScrub02, AudioID::TerrainScrub04), glm::vec3(0.f));

                        if (auto idx = m_playerIndices[m_currentClient][m_currentPlayer]; idx > -1)
                        {
                            playAvatarSoundDelayed(idx, "scrub", glm::vec3(0.f), 2.2f);
                        }
                        break;
                    case TerrainID::Green:
                        if (data.club != ClubID::Putter) //previous shot wasn't from green
                        {
                            playSound(cro::Util::Random::value(AudioID::TerrainGreen01, AudioID::TerrainGreen02), glm::vec3(0.f));

                            if (data.pinDistance < MinHoleDistance //near the hole
                                || data.travelDistance > 10000.f) //landed a long shot
                            {
                                playSoundDelayed(AudioID::Applause, glm::vec3(0.f), 0.8f);
                                applaud();
                                Social::awardXP(XPValues[XPID::Special]);
                            }
                        }
                        break;
                    case TerrainID::Water:
                        playSound(cro::Util::Random::value(AudioID::TerrainWater01, AudioID::TerrainWater03), glm::vec3(0.f));
                        break;
                    case TerrainID::Rough:
                        playSound(cro::Util::Random::value(AudioID::TerrainRough01, AudioID::TerrainRough02), glm::vec3(0.f));
                        break;
                    }
                }
                else
                {
                    if (data.club != ClubID::Putter
                        && cro::Util::Random::value(0, 4) == 0)
                    {
                        //you're poop.
                        playSound(AudioID::TerrainScrub01, glm::vec3(0.f));
                    }

                    if (data.club == ClubID::Putter
                        && (data.terrain == TerrainID::Water || data.terrain == TerrainID::Scrub))
                    {
                        //assume we putt off the green on a putting course
                        playSound(AudioID::TerrainWater03, glm::vec3(0.f));
                    }
                }
                break;
            }
        }
        break;
        case MessageID::CollisionMessage:
        {
            const auto& data = msg.getData<CollisionEvent>();
            if (data.type == CollisionEvent::Begin)
            {
                switch (data.terrain)
                {
                default:
                    playSound(AudioID::Ground, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Water:
                    playSound(AudioID::Water, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Hole:
                    playSound(AudioID::Hole, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Scrub:
                    playSound(AudioID::Scrub, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Stone:
                    playSound(AudioID::Stone, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                }
            }
            else if (data.type == CollisionEvent::End)
            {
                if (data.terrain == TerrainID::Green
                    && data.clubID == ClubID::Putter)
                {
                    playSound(AudioID::Ground, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
                else if (data.terrain == TerrainID::Hole)
                {
                    playSound(AudioID::Hole, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
            }
            else if (data.type == CollisionEvent::NearMiss)
            {
                playSoundDelayed(AudioID::NearMiss, data.position, 0.5f, 1.f, MixerChannel::Effects);
                //playSound(AudioID::NearHole, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
            }
        }
        break;
        case MessageID::SceneMessage:
        {
            const auto& data = msg.getData<SceneEvent>();
            if (data.type == SceneEvent::TransitionComplete)
            {
                m_newHole = true;
            }
        }
        break;
        }
    }
}

void GolfSoundDirector::addAudioScape(const std::string& path, cro::AudioResource& resource)
{
    //we emplace back even if it fails/has empty path so indices match the player indices.
    m_playerVoices.emplace_back().loadFromFile(path, resource);
}

void GolfSoundDirector::setPlayerIndex(std::size_t client, std::size_t player, std::int32_t index)
{
    CRO_ASSERT(index < m_playerVoices.size(), "load audioscapes first!");
    m_playerIndices[client][player] = index;
}

void GolfSoundDirector::setActivePlayer(std::size_t client, std::size_t player)
{
    m_currentClient = client;
    m_currentPlayer = player;

    if (m_newHole)
    {
        std::int32_t honour = (client << 8) | player;
        if (honour != m_honourID)
        {
            playSoundDelayed(AudioID::Honour, glm::vec3(0.f), 1.f, 1.f, MixerChannel::Voice);
        }
        m_honourID = honour;
    }
    m_newHole = false;
}

//private
cro::Entity GolfSoundDirector::playSound(std::int32_t id, glm::vec3 position, float volume)
{
    const auto playDefault = [&, id, volume, position]()
    {
        auto ent = getNextEntity();
        ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[id]);
        ent.getComponent<cro::AudioEmitter>().setVolume(volume);
        ent.getComponent<cro::AudioEmitter>().setPitch(1.f);
        ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Voice);
        ent.getComponent<cro::AudioEmitter>().play();
        ent.getComponent<cro::Transform>().setPosition(position);
        return ent;
    };

    const auto playSpecial = [&, id, volume, position]()
    {
        if (auto idx = m_playerIndices[m_currentClient][m_currentPlayer]; idx > -1)
        {
            std::string emitterName;
            switch (id)
            {
            default: break;
            case AudioID::TerrainBunker01:
                emitterName = "bunker";
                break;
            case AudioID::TerrainFairway01:
                emitterName = "fairway";
                break;
            case AudioID::TerrainGreen01:
                emitterName = "green";
                break;
            case AudioID::TerrainRough01:
                emitterName = "rough";
                break;
            case AudioID::TerrainScrub01:
                emitterName = "scrub";
                break;
            case AudioID::TerrainWater01:
                emitterName = "water";
                break;
            case AudioID::Hook:
                emitterName = "hook";
                break;
            case AudioID::Slice:
                emitterName = "slice";
                break;
            }

            if (m_playerVoices[idx].hasEmitter(emitterName))
            {
                return playAvatarSound(idx, emitterName, position);
            }
        }

        return playDefault();
    };

    switch (id)
    {
    default: 
        return playDefault();
    case AudioID::TerrainBunker01:
    case AudioID::TerrainFairway01:
    case AudioID::TerrainGreen01:
    case AudioID::TerrainRough01:
    case AudioID::TerrainScrub01:
    case AudioID::TerrainWater01:
    case AudioID::Hook:
    case AudioID::Slice:
        return playSpecial();
    }
}

void GolfSoundDirector::playSoundDelayed(std::int32_t id, glm::vec3 position, float delay, float volume, std::uint8_t channel)
{
    auto entity = getScene().createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(delay);
    entity.getComponent<cro::Callback>().function =
        [&, id, position, volume, channel](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0)
        {
            playSound(id, position, volume).getComponent<cro::AudioEmitter>().setMixerChannel(channel);

            e.getComponent<cro::Callback>().active = false;
            getScene().destroyEntity(e);
        }
    };
}

cro::Entity GolfSoundDirector::playAvatarSound(std::int32_t idx, const std::string& emitterName, glm::vec3 position)
{
    auto ent = getNextEntity();
    ent.getComponent<cro::AudioEmitter>() = m_playerVoices[idx].getEmitter(emitterName);
    ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Voice);
    ent.getComponent<cro::AudioEmitter>().setPitch(1.f);
    ent.getComponent<cro::AudioEmitter>().play();
    ent.getComponent<cro::Transform>().setPosition(position);
    return ent;
}

void GolfSoundDirector::playAvatarSoundDelayed(std::int32_t idx, const std::string& emitterName, glm::vec3 position, float delay)
{
    if (m_playerVoices[idx].hasEmitter(emitterName))
    {
        auto entity = getScene().createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(delay);
        entity.getComponent<cro::Callback>().function =
            [&, idx, position, emitterName](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime -= dt;

            if (currTime < 0)
            {
                playAvatarSound(idx, emitterName, position);

                e.getComponent<cro::Callback>().active = false;
                getScene().destroyEntity(e);
            }
        };
    }
}

void GolfSoundDirector::applaud()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Crowd;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<VatAnimation>().applaud();
    };
    getScene().getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::Spectator;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().setUserData<bool>(true);
    };
    getScene().getSystem<cro::CommandSystem>()->sendCommand(cmd);
}
/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "CarSystem.hpp"
#include "Track.hpp"
#include "TrackCamera.hpp"
#include "EndlessConsts.hpp"

#include <crogine/ecs/components/AudioEmitter.hpp>

CarSystem::CarSystem(cro::MessageBus& mb, Track& track, const TrackCamera& cam)
    : cro::System   (mb, typeid(CarSystem)),
    m_track         (track),
    m_camera        (cam)
{
    requireComponent<Car>();
}

//public
void CarSystem::process(float dt)
{
    const auto TrackLength = m_track.getSegmentCount() * SegmentLength;
    const auto CamPos = m_camera.getPosition().x + TrackLength; //offset wrap-around

    auto& entities = getEntities();
    for (auto e : entities)
    {
        auto& car = e.getComponent<Car>();
        //cars on a buffered piece of track (ie pre-generated) are inactive
        if (!car.active)
        {
            continue;
        }
        
        std::size_t oldSeg = static_cast<std::size_t>(std::floor(car.z / SegmentLength));

        //update each cars Z position
        car.z += car.speed * dt;
        if (car.z >= TrackLength)
        {
            car.z -= TrackLength;
        }
        else if (car.z < 0.f)
        {
            car.z += TrackLength;
        }

        //TODO avoid other cars
        //eg if there's a car in front and car is slower, move towards centre of road
        //else move back to edge

        //if we switched segs, remove from old and add to new
        std::size_t newSeg = static_cast<std::size_t>(std::floor(car.z / SegmentLength));
        if (oldSeg != newSeg)
        {
            auto& cars = m_track[oldSeg].cars;
            cars.erase(std::remove_if(cars.begin(), cars.end(), [e](const cro::Entity ent) {return ent == e; }), cars.end());

            m_track[newSeg].cars.push_back(e);
        }

        car.sprite.segmentInterp = std::clamp((car.z - m_track[newSeg].position.z) / SegmentLength, 0.f, 1.f);

        //animate based on current seg curve
        //and which side of the road they're currently on
        //const auto curve = m_track[newSeg].curve;
        //if (curve > 0)
        //{
        //    //hard right
        //    car.sprite.uv = car.frames[Car::Frame::HardRight];
        //}
        //else if (curve < 0)
        //{
        //    //hard left
        //    car.sprite.uv = car.frames[Car::Frame::HardLeft];
        //}
        //else
        //{
        //    //straight so which side of the road?
        //    if (car.sprite.position < -0.5f)
        //    {
        //        //right
        //        car.sprite.uv = car.frames[Car::Frame::Right];
        //    }
        //    else if (car.sprite.position > 0.5f)
        //    {
        //        //left
        //        car.sprite.uv = car.frames[Car::Frame::Left];
        //    }
        //    else
        //    {
        //        //middle
        //        car.sprite.uv = car.frames[Car::Frame::Straight];
        //    }
        //}


        //set audio volume based on distance to player
        static constexpr float MaxDistance = static_cast<float>(DrawDistance / 10);
        const float pos = car.z + TrackLength; //take care of any wrap-around
        const float dist = std::abs( pos - CamPos);
        if (dist > MaxDistance)
        {
            if (e.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Playing)
            {
                e.getComponent<cro::AudioEmitter>().stop();
            }
        }
        else
        {
            const float vol = (dist / MaxDistance);
            e.getComponent<cro::AudioEmitter>().setVolume((vol * vol) * car.baseVolume);
            
            if (e.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
            {
                e.getComponent<cro::AudioEmitter>().play();
            }
            //TODO we could combine cart speed with player speed to calc an
            //approx doppler effect and set the emitter's pitch
        }
    }
}

//private
void CarSystem::onEntityAdded(cro::Entity e)
{
    //calc the animation frames based on the
    //assumption they move from left to right
    auto& car = e.getComponent<Car>();
    auto baseUV = car.sprite.uv;

    car.frames[Car::Frame::HardLeft] = baseUV;
    car.frames[Car::Frame::HardLeft].left -= baseUV.width * 2.f;

    car.frames[Car::Frame::Left] = baseUV;
    car.frames[Car::Frame::Left].left -= baseUV.width;

    car.frames[Car::Frame::Straight] = baseUV;

    car.frames[Car::Frame::Right] = baseUV;
    car.frames[Car::Frame::Right].left += baseUV.width;

    car.frames[Car::Frame::HardRight] = baseUV;
    car.frames[Car::Frame::HardRight].left += baseUV.width * 2.f;
}
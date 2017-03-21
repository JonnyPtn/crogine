/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "MyApp.hpp"
#include "MenuState.hpp"

#include <crogine/system/Clock.hpp>

MyApp::MyApp()
	: m_stateStack({*this, getWindow()})
{
	//register states
	m_stateStack.registerState<MenuState>(States::ID::MainMenu);


	m_stateStack.pushState(States::MainMenu);
}

//public
void MyApp::handleEvent(const cro::Event& evt)
{
	if (evt.type == SDL_KEYUP)
	{
		switch (evt.key.keysym.sym)
		{
		default: break;
		case SDLK_ESCAPE:
		case SDLK_AC_BACK:
			//TODO replace with app::quit()
			getWindow().close();
			break;
		}
	}
	
	m_stateStack.handleEvent(evt);
}

void MyApp::simulate(cro::Time dt)
{
	m_stateStack.simulate(dt);
}

void MyApp::render()
{
	m_stateStack.render();
}
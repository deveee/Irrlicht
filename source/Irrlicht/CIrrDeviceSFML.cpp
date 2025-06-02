// Copyright (C) 2002-2012 Nikolaus Gebhardt
// Copyright (C) 2022-2025 Dawid Gan
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_SFML_DEVICE_

#include "CIrrDeviceSFML.h"
#include "IEventReceiver.h"
#include "irrList.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include "COSOperator.h"
#include <stdio.h>
#include <stdlib.h>
#include "SIrrCreationParameters.h"

#if defined(_IRR_ANDROID_PLATFORM_)
#include <jni.h>
#endif

#if defined(_IRR_IOS_PLATFORM_)
#import <UIKit/UIKit.h>
#elif defined(_IRR_OSX_PLATFORM_)
#import <AppKit/AppKit.h>
#endif

namespace irr
{
	namespace video
	{
#ifdef _IRR_COMPILE_WITH_OPENGL_
		IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
				io::IFileSystem* io, CIrrDeviceSFML* device);
#endif

#if defined(_IRR_COMPILE_WITH_OGLES2_)
		IVideoDriver* createOGLES2Driver(const SIrrlichtCreationParameters& params,
				io::IFileSystem* io, CIrrDeviceSFML* device);
#endif

#if defined(_IRR_COMPILE_WITH_OGLES1_)
		IVideoDriver* createOGLES1Driver(const SIrrlichtCreationParameters& params,
				io::IFileSystem* io, CIrrDeviceSFML* device);
#endif
	} // end namespace video

} // end namespace irr


namespace irr
{

//! constructor
CIrrDeviceSFML::CIrrDeviceSFML(const SIrrlichtCreationParameters& param)
	: CIrrDeviceStub(param),
	Window(0),
	MouseX(0), MouseY(0), MouseButtonStates(0),
	Width(param.WindowSize.Width), Height(param.WindowSize.Height),
	WindowHasFocus(false),
	Resizable(param.WindowResizable == 1),
	AccelerometerIndex(-1), AccelerometerInstance(-1),
	GyroscopeIndex(-1), GyroscopeInstance(-1),
	NativeScaleX(1.0f), NativeScaleY(1.0f),
	IgnoreWarpMouseEvent(false), ShouldUseRelativeMouse(false),
	LongTouchTimer(0), LongTouchX(0), LongTouchY(0), LongTouchHandled(true)
{
#ifdef _DEBUG
	setDebugName("CIrrDeviceSFML");
#endif

#if defined(_IRR_ANDROID_PLATFORM_) || defined(_IRR_IOS_PLATFORM_)
	ShouldUseRelativeMouse = supportsRelativeMouse();
#endif

#if defined(_IRR_OSX_PLATFORM_)
		// Enable AppleMomentumScrollSupported on macOS
		[[NSUserDefaults standardUserDefaults] setBool: YES
							   forKey: @"AppleMomentumScrollSupported"];
#endif

	core::stringc sfmlversion = "SFML Version ";
	sfmlversion += SFML_VERSION_MAJOR;
	sfmlversion += ".";
	sfmlversion += SFML_VERSION_MINOR;
	sfmlversion += ".";
	sfmlversion += SFML_VERSION_PATCH;

	Operator = new COSOperator(sfmlversion, this);
	os::Printer::log(sfmlversion.c_str(), ELL_INFORMATION);

	// create keymap
	createKeyMap();

	if (CreationParams.DriverType != video::EDT_NULL)
	{
		//~ for (int i = 0; i < SDL_NumSensors(); i++)
		//~ {
			//~ if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_ACCEL)
			//~ {
				//~ AccelerometerIndex = i;
			//~ }
			//~ else if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_GYRO)
			//~ {
				//~ GyroscopeIndex = i;
			//~ }
		//~ }

		// create the window, only if we do not use the null device
		bool success = createWindow();

		if (!success)
			return;

		Window->setVerticalSyncEnabled(param.Vsync);
	}

	// create cursor control
	CursorControl = new CCursorControl(this);

	// create driver
	createDriver();

	if (VideoDriver)
		createGUIAndScene();
}


//! destructor
CIrrDeviceSFML::~CIrrDeviceSFML()
{
	//~ for (u32 i = 0; i < Joysticks.size(); i++)
	//~ {
//~ #if defined(_IRR_COMPILE_WITH_SFML_GAMECONTROLLER)
		//~ SDL_GameController* gameController = SDL_GameControllerFromInstanceID(Joysticks[i]);
		//~ SDL_GameControllerClose(gameController);
//~ #elif defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
		//~ SDL_Joystick* joystick = SDL_JoystickFromInstanceID(Joysticks[i]);
		//~ SDL_JoystickClose(joystick);
//~ #endif
	//~ }

	if (Window)
	{
		delete Window;
		Window = NULL;
	}
}

bool CIrrDeviceSFML::createWindow()
{
	if (Close)
		return false;

	// Get native scale before window creation on platforms that support
	// high dpi.
#if defined(_IRR_IOS_PLATFORM_) || defined(_IRR_OSX_PLATFORM_)
	updateNativeScaleFromSystem();
#endif

	// SFML accepts window dimensions equal to 0 only for fullscreen
	// window. Use desktop size in windowed mode.
	if (!CreationParams.Fullscreen && (Width == 0 || Height == 0))
	{
		sf::VideoMode mode = sf::VideoMode::getDesktopMode();

		if (mode.size.x > 0 || mode.size.y > 0)
		{
			Width = roundf((float)mode.size.x * NativeScaleX);
			Height = roundf((float)mode.size.y * NativeScaleY);

		}
		else
		{
			// Shouldn't happen, just in case
			Width = 640;
			Height = 480;
		}
	}

	bool success = createWindowWithContext();

	if (!success)
	{
		if (CreationParams.AntiAlias > 1)
		{
			while (--CreationParams.AntiAlias > 1)
			{
				success = createWindowWithContext();

				if (success)
					break;
			}

			if (!success)
			{
				CreationParams.AntiAlias = 0;

				success = createWindowWithContext();

				if (success)
				{
					os::Printer::log("AntiAliasing disabled due to lack of support!");
				}
			}
		}

		if (!success && CreationParams.Stencilbuffer)
		{
			CreationParams.Stencilbuffer = false;

			success = createWindowWithContext();

			if (success)
			{
				os::Printer::log("Stencilbuffer disabled due to lack of support!");
			}
		}

		if (!success && CreationParams.ZBufferBits > 16)
		{
			while (CreationParams.ZBufferBits > 16)
			{
				CreationParams.ZBufferBits -= 8;

				success = createWindowWithContext();

				if (success)
					break;
			}

			if (success)
			{
				os::Printer::log("Use lower ZBufferBits due to lack of support!");
			}
		}
	}

	if (!success)
	{
		os::Printer::log("Could not create context!");
		return false;
	}

	return true;
}

bool CIrrDeviceSFML::createWindowWithContext()
{
	std::uint32_t sfmlStyle = Resizable ? sf::Style::Default : sf::Style::Close;
	sf::State sfmlState = CreationParams.Fullscreen ? sf::State::Fullscreen : sf::State::Windowed;
	
	sf::ContextSettings ContextSettings;
	
	if (CreationParams.DriverType == video::EDT_OPENGL ||
		CreationParams.DriverType == video::EDT_OGLES2 ||
		CreationParams.DriverType == video::EDT_OGLES1)
	{
		if (CreationParams.DriverType == video::EDT_OGLES2)
		{
			ContextSettings.majorVersion = 3;
			ContextSettings.minorVersion = 0;
		}
		else if (CreationParams.DriverType == video::EDT_OGLES1)
		{
			ContextSettings.majorVersion = 1;
			ContextSettings.minorVersion = 0;
		}
		else
		{
			ContextSettings.majorVersion = 1;
			ContextSettings.minorVersion = 1;
		}

		ContextSettings.depthBits = CreationParams.ZBufferBits;
		ContextSettings.stencilBits = CreationParams.Stencilbuffer ? 8 : 0;
		
		if (CreationParams.AntiAlias > 1)
		{
			ContextSettings.antiAliasingLevel = CreationParams.AntiAlias;
		}
		else
		{
			ContextSettings.antiAliasingLevel = 0;
		}
	}

	sf::VideoMode videoMode({
		static_cast<unsigned int>(roundf((float)Width / NativeScaleX)),
		static_cast<unsigned int>(roundf((float)Height / NativeScaleY))}
	);

	Window = new sf::Window(videoMode, "", sfmlStyle, sfmlState, ContextSettings);
	
	if (!Window || !Window->isOpen())
	{
		if (Window)
		{
			delete Window;
			Window = nullptr;
		}
		
		// Fallback for OGLES2 - try version 2.0
		if (CreationParams.DriverType == video::EDT_OGLES2)
		{
			ContextSettings.majorVersion = 2;
			ContextSettings.minorVersion = 0;
			
			Window = new sf::Window(videoMode, "", sfmlStyle, sfmlState, ContextSettings);
			
			if (!Window || !Window->isOpen())
			{
				if (Window)
				{
					delete Window;
					Window = nullptr;
				}
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	
	updateNativeScale();
	
	// Update dimensions if they were set to 0
	if (CreationParams.WindowSize.Width == 0 || CreationParams.WindowSize.Height == 0)
	{
		sf::Vector2u windowSize = Window->getSize();
		Width = roundf((float)windowSize.x * NativeScaleX);
		Height = roundf((float)windowSize.y * NativeScaleY);
	}
	
	CreationParams.WindowSize.Width = Width;
	CreationParams.WindowSize.Height = Height;
	
	return true;
}

void CIrrDeviceSFML::updateNativeScaleFromSystem()
{
	float scaleFactor = 1.0f;

#if defined(_IRR_IOS_PLATFORM_)
	scaleFactor = UIScreen.mainScreen.scale;
#elif defined(_IRR_OSX_PLATFORM_)
	scaleFactor = [[NSScreen mainScreen] backingScaleFactor];
#endif

	NativeScaleX = scaleFactor;
	NativeScaleY = scaleFactor;
}

void CIrrDeviceSFML::updateNativeScale()
{
	//~ int width = 0;
	//~ int height = 0;
	//~ SDL_GetWindowSize(Window, &width, &height);

	//~ int real_width = width;
	//~ int real_height = height;

	//~ if (CreationParams.DriverType == video::EDT_OPENGL ||
		//~ CreationParams.DriverType == video::EDT_OGLES2 ||
		//~ CreationParams.DriverType == video::EDT_OGLES1)
	//~ {
		//~ SDL_GL_GetDrawableSize(Window, &real_width, &real_height);
	//~ }

	//~ NativeScaleX = (f32)real_width / (f32)width;
	//~ NativeScaleY = (f32)real_height / (f32)height;
}

void CIrrDeviceSFML::setCursorVisible(bool visible)
{
#if defined(_IRR_OSX_PLATFORM_)
	if (visible)
		CGDisplayShowCursor(CGMainDisplayID());
	else
		CGDisplayHideCursor(CGMainDisplayID());
#elif defined(_IRR_ANDROID_PLATFORM_) || defined(_IRR_IOS_PLATFORM_)
	// Hiding cursor on Android has a sense only when relative mouse mode is
	// available because SDL_WarpMouseInWindow doesn't work anyway.
	//~ if (ShouldUseRelativeMouse)
	//~ {
		//~ if (visible)
			//~ SDL_SetRelativeMouseMode(SDL_FALSE);
		//~ else
			//~ SDL_SetRelativeMouseMode(SDL_TRUE);
	//~ }
#else
	if (visible)
	{
		Window->setMouseCursorVisible(true);
//#if defined(_IRR_OSX_PLATFORM_)
//		NSApp.presentationOptions &= ~NSApplicationPresentationDisableCursorLocationAssistance;
//#endif
	}
	else
	{
		Window->setMouseCursorVisible(false);
//#if defined(_IRR_OSX_PLATFORM_)
//		NSApp.presentationOptions |= NSApplicationPresentationDisableCursorLocationAssistance;
//#endif
	}
#endif
}

//! create the driver
void CIrrDeviceSFML::createDriver()
{
	switch(CreationParams.DriverType)
	{
	case video::DEPRECATED_EDT_DIRECT3D8_NO_LONGER_EXISTS:
	case video::EDT_DIRECT3D9:
	case video::EDT_SOFTWARE:
	case video::EDT_BURNINGSVIDEO:
		os::Printer::log("SFML device does not support this driver. Try another one.", ELL_ERROR);
		break;

	case video::EDT_OPENGL:
#ifdef _IRR_COMPILE_WITH_OPENGL_
		VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, this);
#else
		os::Printer::log("No OpenGL support compiled in.", ELL_ERROR);
#endif
		break;

	case video::EDT_OGLES2:
#ifdef _IRR_COMPILE_WITH_OGLES2_
		VideoDriver = video::createOGLES2Driver(CreationParams, FileSystem, this);
#else
		os::Printer::log("No OpenGL ES2 support compiled in.", ELL_ERROR);
#endif
		break;

	case video::EDT_OGLES1:
#ifdef _IRR_COMPILE_WITH_OGLES1_
		VideoDriver = video::createOGLES1Driver(CreationParams, FileSystem, this);
#else
		os::Printer::log("No OpenGL ES1 support compiled in.", ELL_ERROR);
#endif
		break;

	case video::EDT_NULL:
		VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
		break;

	default:
		os::Printer::log("Unable to create video driver of unknown type.", ELL_ERROR);
		break;
	}
}

//! runs the device. Returns false if device wants to be deleted
bool CIrrDeviceSFML::run()
{
	os::Timer::tick();

	if (Close)
		return false;

	SEvent irrevent;

	while (const std::optional sfml_event = Window->pollEvent())
	{
		if (Close)
			break;

		//~ // From https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md
		//~ // However, there's a chance (on older hardware, or on systems under heavy load),
		//~ // where the GL context can not be restored. In that case you have to
		//~ // listen for a specific message (SDL_RENDER_DEVICE_RESET) and restore
		//~ // your textures manually or quit the app.
		//~ case SDL_RENDER_DEVICE_RESET:
			//~ Close = true;
			//~ return false;

		//~ case SDL_SENSORUPDATE:
			//~ if (SDL_event.sensor.which == AccelerometerInstance)
			//~ {
				//~ SDL_DisplayOrientation orientation = SDL_GetDisplayOrientation(0);
				//~ irrevent.EventType = irr::EET_ACCELEROMETER_EVENT;

				//~ if (orientation == SDL_ORIENTATION_LANDSCAPE ||
					//~ orientation == SDL_ORIENTATION_LANDSCAPE_FLIPPED)
				//~ {
					//~ irrevent.AccelerometerEvent.X = SDL_event.sensor.data[0];
					//~ irrevent.AccelerometerEvent.Y = SDL_event.sensor.data[1];
				//~ }
				//~ else
				//~ {
					//~ // For android multi-window mode vertically
					//~ irrevent.AccelerometerEvent.X = -SDL_event.sensor.data[1];
					//~ irrevent.AccelerometerEvent.Y = -SDL_event.sensor.data[0];
				//~ }

				//~ irrevent.AccelerometerEvent.Z = SDL_event.sensor.data[2];

				//~ if (irrevent.AccelerometerEvent.X < 0.0)
				//~ {
					//~ irrevent.AccelerometerEvent.X *= -1.0;
				//~ }

				//~ if (orientation == SDL_ORIENTATION_LANDSCAPE_FLIPPED ||
					//~ orientation == SDL_ORIENTATION_PORTRAIT_FLIPPED)
				//~ {
					//~ irrevent.AccelerometerEvent.Y *= -1.0;
				//~ }

				//~ postEventFromUser(irrevent);
			//~ }
			//~ else if (SDL_event.sensor.which == GyroscopeInstance)
			//~ {
				//~ irrevent.EventType = irr::EET_GYROSCOPE_EVENT;
				//~ irrevent.GyroscopeEvent.X = SDL_event.sensor.data[0];
				//~ irrevent.GyroscopeEvent.Y = SDL_event.sensor.data[1];
				//~ irrevent.GyroscopeEvent.Z = SDL_event.sensor.data[2];
				//~ postEventFromUser(irrevent);
			//~ }
			//~ break;

		if (sfml_event->is<sf::Event::TouchMoved>())
		{
			const auto* touch = sfml_event->getIf<sf::Event::TouchMoved>();
			
			if (TouchIDs.size() == 1)
			{
				if (std::abs(LongTouchX - touch->position.x) > Width * 0.05f ||
					std::abs(LongTouchY - touch->position.y) > Height * 0.05f)
				{
					LongTouchHandled = true;
				}
			}
			irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = irr::ETIE_MOVED;
			irrevent.TouchInput.ID = touch->finger;
			irrevent.TouchInput.X = touch->position.x;
			irrevent.TouchInput.Y = touch->position.y;
			irrevent.TouchInput.touchedCount = TouchIDs.size();
			postEventFromUser(irrevent);
		}
		
		else if (sfml_event->is<sf::Event::TouchBegan>())
		{
			const auto* touch = sfml_event->getIf<sf::Event::TouchBegan>();
			
			// Long touch only for first finger
			if (TouchIDs.size() == 0)
			{
				LongTouchTimer = os::Timer::getTime();
				LongTouchX = touch->position.x;
				LongTouchY = touch->position.y;
				LongTouchHandled = false;
			}
			else
			{
				LongTouchHandled = true;
			}
			
			TouchIDs.insert(touch->finger);
			irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = irr::ETIE_PRESSED_DOWN;
			irrevent.TouchInput.ID = touch->finger;
			irrevent.TouchInput.X = touch->position.x;
			irrevent.TouchInput.Y = touch->position.y;
			irrevent.TouchInput.touchedCount = TouchIDs.size();
		   	postEventFromUser(irrevent);
		}
		
		else if (sfml_event->is<sf::Event::TouchEnded>())
		{
			const auto* touch = sfml_event->getIf<sf::Event::TouchEnded>();
			
			if (TouchIDs.size() == 1)
			{
				LongTouchHandled = true;
			}
			
			irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = irr::ETIE_LEFT_UP;
			irrevent.TouchInput.ID = touch->finger;
			irrevent.TouchInput.X = touch->position.x;
			irrevent.TouchInput.Y = touch->position.y;
			irrevent.TouchInput.touchedCount = TouchIDs.size();
			postEventFromUser(irrevent);
			TouchIDs.erase(touch->finger);
		}

		else if (sfml_event->is<sf::Event::MouseWheelScrolled>())
		{
			const auto* mouseWheelScroll = sfml_event->getIf<sf::Event::MouseWheelScrolled>();
			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = irr::EMIE_MOUSE_WHEEL;
			irrevent.MouseInput.X = MouseX;
			irrevent.MouseInput.Y = MouseY;

#if defined(_IRR_IOS_PLATFORM_) || defined(_IRR_OSX_PLATFORM_)
			irrevent.MouseInput.Control = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LSystem) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RSystem);
#else
			irrevent.MouseInput.Control = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
#endif
			irrevent.MouseInput.Shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

			irrevent.MouseInput.ButtonStates = MouseButtonStates;
			
			if (mouseWheelScroll->wheel == sf::Mouse::Wheel::Vertical)
			{
				irrevent.MouseInput.Wheel = mouseWheelScroll->delta;
			}
			else if (mouseWheelScroll->wheel == sf::Mouse::Wheel::Horizontal)
			{
				irrevent.MouseInput.Wheel = mouseWheelScroll->delta;
			}

			postEventFromUser(irrevent);
		}

		else if (sfml_event->is<sf::Event::MouseMoved>())
		{
			const auto* mouseMove = sfml_event->getIf<sf::Event::MouseMoved>();
#if defined(_IRR_ANDROID_PLATFORM_) || defined(_IRR_IOS_PLATFORM_)
			if (!ShouldUseRelativeMouse)
				break;
#endif

			if (IgnoreWarpMouseEvent)
			{
				IgnoreWarpMouseEvent = false;
				break;
			}

			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;

			MouseX = irrevent.MouseInput.X = mouseMove->position.x * NativeScaleX;
			MouseY = irrevent.MouseInput.Y = mouseMove->position.y * NativeScaleY;

#if defined(_IRR_IOS_PLATFORM_) || defined(_IRR_OSX_PLATFORM_)
			irrevent.MouseInput.Control = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LSystem) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RSystem);
#else
			irrevent.MouseInput.Control = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
#endif
			irrevent.MouseInput.Shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

			irrevent.MouseInput.ButtonStates = MouseButtonStates;

			postEventFromUser(irrevent);
		}

		else if (sfml_event->is<sf::Event::MouseButtonPressed>() ||
			sfml_event->is<sf::Event::MouseButtonReleased>())
		{
			bool mouseButtonPressed; 
			sf::Vector2i mouseButtonPosition;
			sf::Mouse::Button mouseButtonButton;
			
			if (sfml_event->is<sf::Event::MouseButtonPressed>())
			{
				const auto* mouseButton = sfml_event->getIf<sf::Event::MouseButtonPressed>();
				mouseButtonPressed = true;
				mouseButtonPosition = mouseButton->position;
				mouseButtonButton = mouseButton->button;
			}
			else
			{
				const auto* mouseButton = sfml_event->getIf<sf::Event::MouseButtonReleased>();
				mouseButtonPressed = false;
				mouseButtonPosition = mouseButton->position;
				mouseButtonButton = mouseButton->button;
			}

#if defined(_IRR_ANDROID_PLATFORM_) || defined(_IRR_IOS_PLATFORM_)
			if (!ShouldUseRelativeMouse)
				break;
#endif

			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.X = mouseButtonPosition.x * NativeScaleX;
			irrevent.MouseInput.Y = mouseButtonPosition.y * NativeScaleY;

#if defined(_IRR_IOS_PLATFORM_) || defined(_IRR_OSX_PLATFORM_)
			irrevent.MouseInput.Control = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LSystem) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RSystem);
#else
			irrevent.MouseInput.Control = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
#endif
			irrevent.MouseInput.Shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
				sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

			irrevent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;

			switch (mouseButtonButton)
			{
			case sf::Mouse::Button::Left:
				if (mouseButtonPressed)
				{
					irrevent.MouseInput.Event = irr::EMIE_LMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_LEFT;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_LMOUSE_LEFT_UP;
					MouseButtonStates &= ~irr::EMBSM_LEFT;
				}
				break;

			case sf::Mouse::Button::Right:
				if (mouseButtonPressed)
				{
					irrevent.MouseInput.Event = irr::EMIE_RMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_RIGHT;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_RMOUSE_LEFT_UP;
					MouseButtonStates &= ~irr::EMBSM_RIGHT;
				}
				break;

			case sf::Mouse::Button::Middle:
				if (mouseButtonPressed)
				{
					irrevent.MouseInput.Event = irr::EMIE_MMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_MIDDLE;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_MMOUSE_LEFT_UP;
					MouseButtonStates &= ~irr::EMBSM_MIDDLE;
				}
				break;

			default:
				break;
			}

			irrevent.MouseInput.ButtonStates = MouseButtonStates;

			if (irrevent.MouseInput.Event != irr::EMIE_MOUSE_MOVED)
			{
				postEventFromUser(irrevent);

				if ( irrevent.MouseInput.Event >= EMIE_LMOUSE_PRESSED_DOWN && irrevent.MouseInput.Event <= EMIE_MMOUSE_PRESSED_DOWN )
				{
					u32 clicks = checkSuccessiveClicks(irrevent.MouseInput.X, irrevent.MouseInput.Y, irrevent.MouseInput.Event);
					if ( clicks == 2 )
					{
						irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_DOUBLE_CLICK + irrevent.MouseInput.Event-EMIE_LMOUSE_PRESSED_DOWN);
						postEventFromUser(irrevent);
					}
					else if ( clicks == 3 )
					{
						irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_TRIPLE_CLICK + irrevent.MouseInput.Event-EMIE_LMOUSE_PRESSED_DOWN);
						postEventFromUser(irrevent);
					}
				}
			}
		}

		else if (sfml_event->is<sf::Event::KeyPressed>())
		{
			const auto* key_event = sfml_event->getIf<sf::Event::KeyPressed>();
					
			sf::Keyboard::Key key = key_event->code;
			SKeyMap mp;
			mp.Key = key;
			s32 idx = KeyMap.binary_search(mp);
			
			EKEY_CODE keyCode;
			if (idx == -1)
				keyCode = (EKEY_CODE)0;
			else
				keyCode = (EKEY_CODE)KeyMap[idx].IrrKeycode;
			
			irrevent.EventType = irr::EET_KEY_INPUT_EVENT;
			irrevent.KeyInput.Char = 0;
			irrevent.KeyInput.Key = keyCode;
			irrevent.KeyInput.PressedDown = true;
			irrevent.KeyInput.Shift = key_event->shift;
#if defined(_IRR_IOS_PLATFORM_) || defined(_IRR_OSX_PLATFORM_)
			irrevent.KeyInput.Control = key_event->system;
#else
			irrevent.KeyInput.Control = key_event->control;
#endif
			postEventFromUser(irrevent);
		}
		
		else if (sfml_event->is<sf::Event::KeyReleased>())
		{
			const auto* key_event = sfml_event->getIf<sf::Event::KeyReleased>();
					
			sf::Keyboard::Key key = key_event->code;
			SKeyMap mp;
			mp.Key = key;
			s32 idx = KeyMap.binary_search(mp);
			
			EKEY_CODE keyCode;
			if (idx == -1)
				keyCode = (EKEY_CODE)0;
			else
				keyCode = (EKEY_CODE)KeyMap[idx].IrrKeycode;
			
			irrevent.EventType = irr::EET_KEY_INPUT_EVENT;
			irrevent.KeyInput.Char = 0;
			irrevent.KeyInput.Key = keyCode;
			irrevent.KeyInput.PressedDown = false;
			irrevent.KeyInput.Shift = key_event->shift;
#if defined(_IRR_IOS_PLATFORM_) || defined(_IRR_OSX_PLATFORM_)
			irrevent.KeyInput.Control = key_event->system;
#else
			irrevent.KeyInput.Control = key_event->control;
#endif
			postEventFromUser(irrevent);
		}

//~ #if defined(_IRR_COMPILE_WITH_SDL_GAMECONTROLLER)
		//~ case SDL_CONTROLLERBUTTONDOWN:
		//~ case SDL_CONTROLLERBUTTONUP:
			//~ {
				//~ irrevent.EventType = irr::EET_SDL_CONTROLLER_BUTTON_EVENT;
				//~ irrevent.SDLControllerButtonEvent.Joystick = SDL_event.cbutton.which;
				//~ irrevent.SDLControllerButtonEvent.Button = SDL_event.cbutton.button;
				//~ irrevent.SDLControllerButtonEvent.Pressed = SDL_event.cbutton.state;
				//~ postEventFromUser(irrevent);
			//~ }
			//~ break;

		//~ case SDL_CONTROLLERDEVICEADDED:
			//~ {
				//~ int index = SDL_event.cdevice.which;

				//~ SDL_GameController* gameController = SDL_GameControllerOpen(index);

				//~ if (gameController)
				//~ {
					//~ SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gameController);
					//~ SDL_JoystickID instanceId = SDL_JoystickInstanceID(joystick);
					//~ Joysticks.push_back(instanceId);
				//~ }
			//~ }
			//~ break;

		//~ case SDL_CONTROLLERDEVICEREMOVED:
			//~ {
				//~ SDL_JoystickID instanceId = SDL_event.cdevice.which;

				//~ for (u32 i = 0; i < Joysticks.size(); i++)
				//~ {
					//~ if (instanceId != Joysticks[i])
						//~ continue;

					//~ SDL_GameController* gameController = SDL_GameControllerFromInstanceID(instanceId);
					//~ SDL_GameControllerClose(gameController);
					//~ Joysticks.erase(i);
					//~ break;
				//~ }
			//~ }
			//~ break;
//~ #endif

		else if (sfml_event->is<sf::Event::Closed>())
		{
			Close = true;
			return false;
		}
		
		else if (sfml_event->is<sf::Event::Resized>())	
		{
			const auto* size = sfml_event->getIf<sf::Event::Resized>();
			
			updateNativeScale();
			u32 new_width = roundf((float)size->size.x * NativeScaleX);
			u32 new_height = roundf((float)size->size.y * NativeScaleY);
			if (new_width != Width || new_height != Height)
			{
				Width = new_width;
				Height = new_height;
				if (VideoDriver)
					VideoDriver->OnResize(core::dimension2d<u32>(Width, Height));
			}
		}
		
		else if (sfml_event->is<sf::Event::FocusGained>())
		{
			WindowHasFocus = true;
		}
		
		else if (sfml_event->is<sf::Event::FocusLost>())
		{
			WindowHasFocus = false;
		}

		else if (sfml_event->is<sf::Event::TextEntered>())
		{
			const auto* text = sfml_event->getIf<sf::Event::TextEntered>();
			
			irrevent.EventType = irr::EET_SDL_TEXT_EVENT;
			irrevent.SDLTextEvent.Type = irr::ESDLET_TEXTINPUT;
			
			sf::String sfStr(text->unicode);
   			sf::U8String utf8 = sfStr.toUtf8();
			const size_t size = sizeof(irrevent.SDLTextEvent.Text);
			memset(irrevent.SDLTextEvent.Text, 0, size);
			
			if (utf8.length() < size) {
				memcpy(irrevent.SDLTextEvent.Text, utf8.c_str(), utf8.length());
			}
			
			irrevent.SDLTextEvent.Start = 0;
			irrevent.SDLTextEvent.Length = 0;
			postEventFromUser(irrevent);
		}
	} // end while

//~ #if defined(_IRR_COMPILE_WITH_SFML_GAMECONTROLLER)
	//~ for (u32 i = 0; i < Joysticks.size(); i++)
	//~ {
		//~ SDL_GameController* gameController = SDL_GameControllerFromInstanceID(Joysticks[i]);

		//~ if (gameController)
		//~ {
			//~ SEvent irrevent;
			//~ irrevent.EventType = EET_SDL_CONTROLLER_AXIS_EVENT;
			//~ irrevent.SDLControllerAxisEvent.Joystick = Joysticks[i];

			//~ for (s32 j = 0; j < 6; j++)
			//~ {
				//~ irrevent.SDLControllerAxisEvent.Axis[j] = j;
				//~ irrevent.SDLControllerAxisEvent.Value[j] = SDL_GameControllerGetAxis(gameController, (SDL_GameControllerAxis)j);
			//~ }

			//~ postEventFromUser(irrevent);
		//~ }
	//~ }

//~ #elif defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	//~ // TODO: Check if the multiple open/close calls are too expensive, then
	//~ // open/close in the constructor/destructor instead

	//~ // update joystick states manually
	//~ SDL_JoystickUpdate();
	//~ // we'll always send joystick input events...
	//~ SEvent joyevent;
	//~ joyevent.EventType = EET_JOYSTICK_INPUT_EVENT;
	//~ for (u32 i=0; i<Joysticks.size(); ++i)
	//~ {
		//~ SDL_Joystick* joystick = SDL_JoystickFromInstanceID(Joysticks[i]);
		//~ if (joystick)
		//~ {
			//~ int j;
			//~ // query all buttons
			//~ const int numButtons = core::min_(SDL_JoystickNumButtons(joystick), 32);
			//~ joyevent.JoystickEvent.ButtonStates=0;
			//~ for (j=0; j<numButtons; ++j)
				//~ joyevent.JoystickEvent.ButtonStates |= (SDL_JoystickGetButton(joystick, j)<<j);

			//~ // query all axes, already in correct range
			//~ const int numAxes = core::min_(SDL_JoystickNumAxes(joystick), (int)SEvent::SJoystickEvent::NUMBER_OF_AXES);
			//~ joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_X]=0;
			//~ joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Y]=0;
			//~ joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Z]=0;
			//~ joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_R]=0;
			//~ joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_U]=0;
			//~ joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_V]=0;
			//~ for (j=0; j<numAxes; ++j)
				//~ joyevent.JoystickEvent.Axis[j] = SDL_JoystickGetAxis(joystick, j);

			//~ // we can only query one hat, SDL only supports 8 directions
			//~ if (SDL_JoystickNumHats(joystick)>0)
			//~ {
				//~ switch (SDL_JoystickGetHat(joystick, 0))
				//~ {
					//~ case SDL_HAT_UP:
						//~ joyevent.JoystickEvent.POV=0;
						//~ break;
					//~ case SDL_HAT_RIGHTUP:
						//~ joyevent.JoystickEvent.POV=4500;
						//~ break;
					//~ case SDL_HAT_RIGHT:
						//~ joyevent.JoystickEvent.POV=9000;
						//~ break;
					//~ case SDL_HAT_RIGHTDOWN:
						//~ joyevent.JoystickEvent.POV=13500;
						//~ break;
					//~ case SDL_HAT_DOWN:
						//~ joyevent.JoystickEvent.POV=18000;
						//~ break;
					//~ case SDL_HAT_LEFTDOWN:
						//~ joyevent.JoystickEvent.POV=22500;
						//~ break;
					//~ case SDL_HAT_LEFT:
						//~ joyevent.JoystickEvent.POV=27000;
						//~ break;
					//~ case SDL_HAT_LEFTUP:
						//~ joyevent.JoystickEvent.POV=31500;
						//~ break;
					//~ case SDL_HAT_CENTERED:
					//~ default:
						//~ joyevent.JoystickEvent.POV=65535;
						//~ break;
				//~ }
			//~ }
			//~ else
			//~ {
				//~ joyevent.JoystickEvent.POV=65535;
			//~ }

			//~ // we map the number directly
			//~ joyevent.JoystickEvent.Joystick=static_cast<u8>(i);
			//~ // now post the event
			//~ postEventFromUser(joyevent);
			//~ // and close the joystick
		//~ }
	//~ }
//~ #endif

	if (os::Timer::getTime() > LongTouchTimer + 1000 && !LongTouchHandled)
	{
		LongTouchHandled = true;

		SEvent irrevent;
		irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
		irrevent.TouchInput.Event = irr::ETIE_PRESSED_LONG;
		irrevent.TouchInput.ID = *(TouchIDs.begin());
		irrevent.TouchInput.X = LongTouchX;
		irrevent.TouchInput.Y = LongTouchY;
		irrevent.TouchInput.touchedCount = TouchIDs.size();
		postEventFromUser(irrevent);
	}

	return !Close;
}

//! Activate any joysticks, and generate events for them.
bool CIrrDeviceSFML::activateJoysticks(core::array<SJoystickInfo> & joystickInfo)
{
//~ #if defined(_IRR_COMPILE_WITH_SFML_GAMECONTROLLER)
	//~ return true;

//~ #elif defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	//~ joystickInfo.clear();

	//~ // we can name up to 256 different joysticks
	//~ const int numJoysticks = core::min_(SDL_NumJoysticks(), 256);
	//~ Joysticks.reallocate(numJoysticks);
	//~ joystickInfo.reallocate(numJoysticks);

	//~ for (int i = 0; i < numJoysticks; i++)
	//~ {
		//~ SDL_Joystick* joystick = SDL_JoystickOpen(i);
		//~ SDL_JoystickID instanceId = SDL_JoystickInstanceID(joystick);
		//~ Joysticks.push_back(instanceId);

		//~ SJoystickInfo info;
		//~ info.Joystick = i;
		//~ info.Axes = SDL_JoystickNumAxes(joystick);
		//~ info.Buttons = SDL_JoystickNumButtons(joystick);
		//~ info.Name = SDL_JoystickNameForIndex(i);

		//~ if (SDL_JoystickNumHats(joystick) > 0)
			//~ info.PovHat = SJoystickInfo::POV_HAT_PRESENT;
		//~ else
			//~ info.PovHat = SJoystickInfo::POV_HAT_ABSENT;

		//~ joystickInfo.push_back(info);
	//~ }

	//~ for(u32 i = 0; i < joystickInfo.size(); i++)
	//~ {
		//~ char logString[256];
		//~ sprintf(logString, "Found joystick %d, %d axes, %d buttons '%s'",
				//~ i, joystickInfo[i].Axes, joystickInfo[i].Buttons,
				//~ joystickInfo[i].Name.c_str());
		//~ os::Printer::log(logString, ELL_INFORMATION);
	//~ }

	//~ return true;

//~ #endif

	return false;
}



//! pause execution temporarily
void CIrrDeviceSFML::yield()
{
	sf::sleep(sf::milliseconds(0));
}


//! pause execution for a specified time
void CIrrDeviceSFML::sleep(u32 timeMs, bool pauseTimer)
{
	const bool wasStopped = Timer ? Timer->isStopped() : true;
	if (pauseTimer && !wasStopped)
		Timer->stop();
		
	sf::sleep(sf::milliseconds(timeMs));
	
	if (pauseTimer && !wasStopped)
		Timer->start();
}


//! sets the caption of the window
void CIrrDeviceSFML::setWindowCaption(const wchar_t* text)
{
	size_t length = wcslen(text);
	char* textc = new char[length * sizeof(wchar_t) + 1]();
	irr::core::wcharToUtf8(text, textc, length * sizeof(wchar_t) + 1);
	Window->setTitle(textc);
	delete[] textc;
}


//! presents a surface in the client area
bool CIrrDeviceSFML::present(video::IImage* surface, void* windowId, core::rect<s32>* srcClip)
{
	return false;
}


//! notifies the device that it should close itself
void CIrrDeviceSFML::closeDevice()
{
	Close = true;
}


//! \return Pointer to a list with all video modes supported
video::IVideoModeList* CIrrDeviceSFML::getVideoModeList()
{
	if (!VideoModeList->getVideoModeCount())
	{
		std::vector<sf::VideoMode> modes = sf::VideoMode::getFullscreenModes();
		
		if (modes.empty())
		{
			os::Printer::log("No display modes available", ELL_ERROR);
			return VideoModeList;
		}
		
		sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
		VideoModeList->setDesktop(desktopMode.bitsPerPixel,
			core::dimension2d<u32>(desktopMode.size.x, desktopMode.size.y));
		
		for (const sf::VideoMode& mode : modes)
		{
			if (mode.isValid())
			{
				VideoModeList->addMode(core::dimension2d<u32>(mode.size.x, mode.size.y),
					mode.bitsPerPixel);
			}
		}
	}

	return VideoModeList;
}

//! Sets if the window should be resizable in windowed mode.
void CIrrDeviceSFML::setResizable(bool resize)
{
}


//! Minimizes window if possible
void CIrrDeviceSFML::minimizeWindow()
{
}


//! Maximize window
void CIrrDeviceSFML::maximizeWindow()
{
}

//! Get the position of this window on screen
core::position2di CIrrDeviceSFML::getWindowPosition()
{
	sf::Vector2i position = Window->getPosition();
	int x = position.x;
	int y = position.y;

	return core::position2di(x, y);
}


//! Restore original window size
void CIrrDeviceSFML::restoreWindow()
{
}

bool CIrrDeviceSFML::isFullscreen() const
{
	return CIrrDeviceStub::isFullscreen();
}


//! returns if window is active. if not, nothing need to be drawn
bool CIrrDeviceSFML::isWindowActive() const
{
	return WindowHasFocus;
}


//! returns if window has focus.
bool CIrrDeviceSFML::isWindowFocused() const
{
	return WindowHasFocus;
}


//! returns if window is minimized.
bool CIrrDeviceSFML::isWindowMinimized() const
{
	return !WindowHasFocus;
}


//! Set the current Gamma Value for the Display
bool CIrrDeviceSFML::setGammaRamp( f32 red, f32 green, f32 blue, f32 brightness, f32 contrast )
{
	return false;
}

//! Get the current Gamma Value for the Display
bool CIrrDeviceSFML::getGammaRamp( f32 &red, f32 &green, f32 &blue, f32 &brightness, f32 &contrast )
{
	return false;
}

//! gets text from the clipboard
//! \return Returns empty string on failure.
const c8* CIrrDeviceSFML::getTextFromClipboard() const
{
	static std::string str;
	str = sf::Clipboard::getString().toAnsiString();
	return str.c_str();
}

//! copies text to the clipboard
void CIrrDeviceSFML::copyToClipboard(const c8* text) const
{
	sf::Clipboard::setString(text);
}

//! returns color format of the window.
video::ECOLOR_FORMAT CIrrDeviceSFML::getColorFormat() const
{
	if (Window)
	{
		sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
		u32 bitsPerPixel = desktop.bitsPerPixel;
		
		if (bitsPerPixel == 16)
		{
			return video::ECF_R5G6B5;
		}
		else if (bitsPerPixel == 24)
		{
			return video::ECF_R8G8B8;
		}
		else // 32-bit
		{
			return video::ECF_A8R8G8B8;
		}
	}
	else
		return CIrrDeviceStub::getColorFormat();
}


void CIrrDeviceSFML::createKeyMap()
{
	KeyMap.reallocate(136);

	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Backspace, KEY_BACK));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Tab, KEY_TAB));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Enter, KEY_RETURN));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Pause, KEY_PAUSE));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Escape, KEY_ESCAPE));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Space, KEY_SPACE));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::PageUp, KEY_PRIOR));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::PageDown, KEY_NEXT));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::End, KEY_END));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Home, KEY_HOME));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Left, KEY_LEFT));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Up, KEY_UP));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Right, KEY_RIGHT));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Down, KEY_DOWN));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Insert, KEY_INSERT));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Delete, KEY_DELETE));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num0, KEY_KEY_0));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num1, KEY_KEY_1));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num2, KEY_KEY_2));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num3, KEY_KEY_3));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num4, KEY_KEY_4));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num5, KEY_KEY_5));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num6, KEY_KEY_6));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num7, KEY_KEY_7));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num8, KEY_KEY_8));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Num9, KEY_KEY_9));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::A, KEY_KEY_A));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::B, KEY_KEY_B));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::C, KEY_KEY_C));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::D, KEY_KEY_D));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::E, KEY_KEY_E));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F, KEY_KEY_F));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::G, KEY_KEY_G));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::H, KEY_KEY_H));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::I, KEY_KEY_I));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::J, KEY_KEY_J));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::K, KEY_KEY_K));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::L, KEY_KEY_L));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::M, KEY_KEY_M));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::N, KEY_KEY_N));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::O, KEY_KEY_O));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::P, KEY_KEY_P));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Q, KEY_KEY_Q));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::R, KEY_KEY_R));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::S, KEY_KEY_S));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::T, KEY_KEY_T));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::U, KEY_KEY_U));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::V, KEY_KEY_V));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::W, KEY_KEY_W));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::X, KEY_KEY_X));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Y, KEY_KEY_Y));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Z, KEY_KEY_Z));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::LSystem, KEY_LWIN));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::RSystem, KEY_RWIN));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad0, KEY_NUMPAD0));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad1, KEY_NUMPAD1));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad2, KEY_NUMPAD2));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad3, KEY_NUMPAD3));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad4, KEY_NUMPAD4));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad5, KEY_NUMPAD5));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad6, KEY_NUMPAD6));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad7, KEY_NUMPAD7));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad8, KEY_NUMPAD8));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Numpad9, KEY_NUMPAD9));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Multiply, KEY_MULTIPLY));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Period, KEY_PERIOD));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Add, KEY_ADD));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Subtract, KEY_SUBTRACT));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Divide, KEY_DIVIDE));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Enter, KEY_RETURN));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Period, KEY_PERIOD));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F1,  KEY_F1));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F2,  KEY_F2));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F3,  KEY_F3));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F4,  KEY_F4));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F5,  KEY_F5));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F6,  KEY_F6));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F7,  KEY_F7));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F8,  KEY_F8));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F9,  KEY_F9));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F10, KEY_F10));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F11, KEY_F11));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F12, KEY_F12));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F13, KEY_F13));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F14, KEY_F14));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::F15, KEY_F15));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::LShift, KEY_LSHIFT));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::RShift, KEY_RSHIFT));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::LControl, KEY_LCONTROL));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::RControl, KEY_RCONTROL));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::LAlt, KEY_LMENU));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::RAlt, KEY_RMENU));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Menu, KEY_MENU));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Comma, KEY_COMMA));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Hyphen, KEY_MINUS));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Escape, KEY_ESCAPE));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Equal, KEY_PLUS));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Semicolon, KEY_OEM_1));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Slash, KEY_OEM_2));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::LBracket, KEY_OEM_4));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::Backslash, KEY_OEM_5));
	KeyMap.push_back(SKeyMap(sf::Keyboard::Key::RBracket, KEY_OEM_6));

	KeyMap.sort();
}

bool CIrrDeviceSFML::activateAccelerometer(float updateInterval)
{
	if (AccelerometerInstance == -1 && AccelerometerIndex != -1)
	{
		//~ SDL_Sensor* accel = SDL_SensorOpen(AccelerometerIndex);

		//~ if (accel)
			//~ AccelerometerInstance = SDL_SensorGetInstanceID(accel);
	}

	return AccelerometerInstance != -1;
}

bool CIrrDeviceSFML::deactivateAccelerometer()
{
	if (AccelerometerInstance == -1)
		return false;

	//~ SDL_Sensor* accel = SDL_SensorFromInstanceID(AccelerometerInstance);

	//~ if (!accel)
		//~ return false;

	//~ SDL_SensorClose(accel);
	AccelerometerInstance = -1;

	return true;
}

bool CIrrDeviceSFML::isAccelerometerActive()
{
	return AccelerometerInstance != -1;
}

bool CIrrDeviceSFML::isAccelerometerAvailable()
{
	return AccelerometerIndex != -1;
}

bool CIrrDeviceSFML::activateGyroscope(float updateInterval)
{
	if (GyroscopeInstance == -1 && GyroscopeIndex != -1)
	{
		//~ SDL_Sensor* gyro = SDL_SensorOpen(GyroscopeIndex);

		//~ if (gyro)
			//~ GyroscopeInstance = SDL_SensorGetInstanceID(gyro);
	}

	return GyroscopeInstance != -1;
}

bool CIrrDeviceSFML::deactivateGyroscope()
{
	if (GyroscopeInstance == -1)
		return false;

	//~ SDL_Sensor* gyro = SDL_SensorFromInstanceID(GyroscopeInstance);

	//~ if (!gyro)
		//~ return false;

	//~ SDL_SensorClose(gyro);
	GyroscopeInstance = -1;

	return true;
}

bool CIrrDeviceSFML::isGyroscopeActive()
{
	return GyroscopeInstance != -1;
}

bool CIrrDeviceSFML::isGyroscopeAvailable()
{
	return GyroscopeIndex != -1;
}

bool CIrrDeviceSFML::supportsRelativeMouse()
{
#if defined(_IRR_ANDROID_PLATFORM_)
	//~ JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();

	//~ if (!env)
		//~ return false;

	//~ jobject activity = (jobject)SDL_AndroidGetActivity();

	//~ if (!activity)
		//~ return false;

	//~ jclass activityClass = env->GetObjectClass(activity);

	//~ if (!activityClass)
		//~ return false;

	//~ jmethodID supportsRelativeMouse = env->GetStaticMethodID(activityClass, "supportsRelativeMouse", "()Z");

	//~ if (!supportsRelativeMouse)
		//~ return false;

	//~ return env->CallStaticBooleanMethod(activityClass, supportsRelativeMouse);
	
	return false;

#elif defined(_IRR_IOS_PLATFORM_)
	if (@available(iOS 14.1, *))
		return true;
	else
		return false;

#else
	return true;
#endif
}

void CIrrDeviceSFML::CCursorControl::initCursors()
{
	//~ Cursors.reallocate(gui::ECI_COUNT);

	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));     // ECI_NORMAL
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR)); // ECI_CROSS
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));      // ECI_HAND
	//~ Cursors.push_back(nullptr);                                             // ECI_HELP
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));     // ECI_IBEAM
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO));        // ECI_NO
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT));      // ECI_WAIT
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL));   // ECI_SIZEALL
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW));  // ECI_SIZENESW
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE));  // ECI_SIZENWSE
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS));    // ECI_SIZENS
	//~ Cursors.push_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE));    // ECI_SIZEWE
	//~ Cursors.push_back(nullptr);                                             // ECI_UP
}

CIrrDeviceSFML::CCursorControl::~CCursorControl()
{
	//~ const u32 count = Cursors.size();
	//~ for (u32 i = 0; i < count; i++)
		//~ SDL_FreeCursor(Cursors[i]);
}

void CIrrDeviceSFML::CCursorControl::setActiveIcon(gui::ECURSOR_ICON iconId)
{
	//~ ActiveIcon = iconId;
	//~ if (iconId >= Cursors.size() || !Cursors[iconId])
	//~ {
		//~ iconId = gui::ECI_NORMAL;
		//~ if (iconId >= Cursors.size() || !Cursors[iconId])
			//~ return;
	//~ }
	//~ SDL_SetCursor(Cursors[iconId]);
}

} // end namespace irr

#endif // _IRR_COMPILE_WITH_SFML_DEVICE_


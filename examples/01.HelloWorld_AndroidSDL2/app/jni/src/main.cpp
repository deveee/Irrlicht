/** Example 001 Helloworld_AndroidSDL2
	This example shows a simple application for Android.
*/

#include <irrlicht.h>

#ifdef _IRR_ANDROID_PLATFORM_

#include <SDL.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

enum GUI_IDS
{
	GUI_INFO_FPS,
	GUI_IRR_LOGO,
};

/*
	Android is using multitouch events.
	We allow users to move around the Irrlicht logo as example of how to use those.
*/
class MyEventReceiver : public IEventReceiver
{
public:
	MyEventReceiver()
	: Device(0), SpriteToMove(0), TouchID(-1)
	{
	}

	void Init(IrrlichtDevice *device)
	{
		Device = device;
	}

	virtual bool OnEvent(const SEvent& event)
	{
		if (event.EventType == EET_TOUCH_INPUT_EVENT)
		{
			switch (event.TouchInput.Event)
			{
				case ETIE_PRESSED_DOWN:
				{
					if ( TouchID == -1 )
					{
						if (Device)
						{
							position2d<s32> touchPoint(event.TouchInput.X, event.TouchInput.Y);
							IGUIElement * logo = Device->getGUIEnvironment()->getRootGUIElement()->getElementFromId ( GUI_IRR_LOGO );
							if ( logo && logo->isPointInside (touchPoint) )
							{
								TouchID = event.TouchInput.ID;
								SpriteToMove = logo;
								SpriteStartRect =  SpriteToMove->getRelativePosition();
								TouchStartPos = touchPoint;
							}
						}
					}
					break;
				}
				case ETIE_MOVED:
					if ( TouchID == event.TouchInput.ID )
					{
						if ( SpriteToMove && TouchID == event.TouchInput.ID )
						{

							position2d<s32> touchPoint(event.TouchInput.X, event.TouchInput.Y);
							MoveSprite(touchPoint);
						}
					}
					break;
				case ETIE_LEFT_UP:
					if ( TouchID == event.TouchInput.ID )
					{
						if ( SpriteToMove )
						{
							TouchID = -1;
							position2d<s32> touchPoint(event.TouchInput.X, event.TouchInput.Y);
							MoveSprite(touchPoint);
							SpriteToMove = 0;
						}
					}
					break;
				default:
					break;
			}
		}

		return false;
	}

	void MoveSprite(const irr::core::position2d<irr::s32> &touchPos)
	{
		irr::core::position2d<irr::s32> move(touchPos-TouchStartPos);
		SpriteToMove->setRelativePosition(SpriteStartRect.UpperLeftCorner + move);
	}

private:
	IrrlichtDevice * Device;
	gui::IGUIElement * SpriteToMove;
	core::rect<s32> SpriteStartRect;
	core::position2d<irr::s32> TouchStartPos;
	s32 TouchID;
};

bool fileExists(stringc filename)
{
	SDL_RWops* file = SDL_RWFromFile(filename.c_str(), "rb");

	bool exists = (file != NULL);
	
	if (file)
	{
		SDL_RWclose(file);
	}
	
	return exists;
}

bool extractFile(stringc filename)
{
	stringc internalStoragePath = SDL_AndroidGetInternalStoragePath();
	stringc output_file_path = internalStoragePath + "/" + filename;
	
	if (fileExists(output_file_path))
		return true;
	
	SDL_RWops* asset = SDL_RWFromFile(filename.c_str(), "rb");
	
	if (asset == NULL)
		return false;

	SDL_RWops* out_file = SDL_RWFromFile(output_file_path.c_str(), "wb");
	
	if (!out_file)
	{
		SDL_RWclose(asset);
		return false;
	}
	
	const int buf_size = 65536;
	char* buf = new char[buf_size]();

	while (true)
	{
		int nb_read = SDL_RWread(asset, buf, 1, buf_size);
		
		if (nb_read < 1)
			break;
			
		int nb_written = SDL_RWwrite(out_file, buf, 1, nb_read);
		
		if (nb_written != nb_read)
		{
			delete[] buf;
			SDL_RWclose(asset);
			SDL_RWclose(out_file);
			return false;
		}
	}

	delete[] buf;
	SDL_RWclose(out_file);
	SDL_RWclose(asset);
	
	return true;
}

void extractAssets()
{
	stringc internalStoragePath = SDL_AndroidGetInternalStoragePath();
	
	mkdir((internalStoragePath + "/media").c_str(), 0755);
	mkdir((internalStoragePath + "/media/Shaders").c_str(), 0755);
	
	extractFile("media/Shaders/COGLES2DetailMap.fsh");
	extractFile("media/Shaders/COGLES2LightmapAdd.fsh");
	extractFile("media/Shaders/COGLES2LightmapModulate.fsh");
	extractFile("media/Shaders/COGLES2NormalMap.fsh");
	extractFile("media/Shaders/COGLES2NormalMap.vsh");
	extractFile("media/Shaders/COGLES2OneTextureBlend.fsh");
	extractFile("media/Shaders/COGLES2ParallaxMap.fsh");
	extractFile("media/Shaders/COGLES2ParallaxMap.vsh");
	extractFile("media/Shaders/COGLES2Reflection2Layer.fsh");
	extractFile("media/Shaders/COGLES2Reflection2Layer.vsh");
	extractFile("media/Shaders/COGLES2Renderer2D.fsh");
	extractFile("media/Shaders/COGLES2Renderer2D_noTex.fsh");
	extractFile("media/Shaders/COGLES2Renderer2D.vsh");
	extractFile("media/Shaders/COGLES2Solid2Layer.fsh");
	extractFile("media/Shaders/COGLES2Solid2.vsh");
	extractFile("media/Shaders/COGLES2Solid.fsh");
	extractFile("media/Shaders/COGLES2Solid.vsh");
	extractFile("media/Shaders/COGLES2SphereMap.fsh");
	extractFile("media/Shaders/COGLES2SphereMap.vsh");
	extractFile("media/Shaders/COGLES2TransparentAlphaChannel.fsh");
	extractFile("media/Shaders/COGLES2TransparentAlphaChannelRef.fsh");
	extractFile("media/Shaders/COGLES2TransparentVertexAlpha.fsh");
	
	extractFile("media/axe.jpg");
	extractFile("media/bigfont.png");
	extractFile("media/dwarf.jpg");
	extractFile("media/dwarf.x");
	extractFile("media/fonthaettenschweiler.bmp");
	extractFile("media/irrlichtlogo3.png");
}

/* Mainloop.
*/
void mainloop( IrrlichtDevice *device, IGUIStaticText * infoText )
{
	u32 loop = 0;	// loop is reset when the app is destroyed unlike runCounter
	static u32 runCounter = 0;	// static's seem to survive even an app-destroy message (not sure if that's guaranteed).
	while(device->run())
	{
		/*
			The window seems to be always active in this setup.
			That's because when it's not active Android will stop the code from running.
		*/
		if (device->isWindowActive())
		{
			/*
				Show FPS and some counters to show which parts of an app run
				in different app-lifecycle states.
			*/
			if ( infoText )
			{
				stringw str = L"FPS:";
				str += (s32)device->getVideoDriver()->getFPS();
				str += L" r:";
				str += runCounter;
				str += L" l:";
				str += loop;
				infoText->setText ( str.c_str() );
			}

			device->getVideoDriver()->beginScene(true, true, SColor(0,100,100,100));
			device->getSceneManager()->drawAll();
			device->getGUIEnvironment()->drawAll();
			device->getVideoDriver()->endScene ();
		}
		device->yield(); // probably nicer to the battery
		++runCounter;
		++loop;
	}
}

/* Main application code. */
int main(int argc, char *argv[])
{
	// Extract assets to internal storage, so that the files can be accessed by app
	extractAssets();
	
	stringc internalStoragePath = SDL_AndroidGetInternalStoragePath();
	
	/*
		The receiver can already receive system events while createDeviceEx is called.
		So we create it first.
	*/
	MyEventReceiver receiver;

	/*
		Create the device.
		EDT_OGLES2 is a shader pipeline. Irrlicht comes with shaders to simulate
				   typical fixed function materials. For this to work the
				   corresponding shaders from the Irrlicht media/Shaders folder are
				   copied to the application assets folder (done in the Makefile).
	*/
	SIrrlichtCreationParameters param;
	param.DriverType = EDT_OGLES2;				// android:glEsVersion in AndroidManifest.xml should be "0x00020000"
	param.WindowSize = dimension2d<u32>(0,0);	// using 0,0 it will automatically set it to the maximal size
	param.Fullscreen = true;
	param.PrivateData = 0;
	param.Bits = 24;
	param.ZBufferBits = 16;
	param.AntiAlias  = 0;
	param.OGLES2ShaderPath = internalStoragePath + "/media/Shaders/";
	param. EventReceiver = &receiver;

	/* Logging is written to a file. So your application should disable all logging when you distribute your
	   application or it can fill up that file over time.
	*/
#ifndef _DEBUG
	param.LoggingLevel = ELL_NONE;
#endif

	IrrlichtDevice *device = createDeviceEx(param);
	if (device == 0)
	   	return 1;

	receiver.Init(device);

	IVideoDriver* driver = device->getVideoDriver();
	ISceneManager* smgr = device->getSceneManager();
	IGUIEnvironment* guienv = device->getGUIEnvironment();
	ILogger* logger = device->getLogger();
	IFileSystem * fs = device->getFileSystem();

	float ddpi = 0;
	SDL_GetDisplayDPI(0, &ddpi, NULL, NULL);

	/* Set the font-size depending on your device.
	   dpi=dots per inch. 1 inch = 2.54 cm. */
	IGUISkin* skin = guienv->getSkin();
	IGUIFont* font = 0;
	if ( ddpi < 100 )	// just guessing some value where fontsize might start to get too small
		font = guienv->getFont(internalStoragePath + "/media/fonthaettenschweiler.bmp");
	else
		font = guienv->getFont(internalStoragePath + "/media/bigfont.png");
	if (font)
		skin->setFont(font);

	// A field to show some text. Comment out stat->setText in run() if you want to see the dpi instead of the fps.
	IGUIStaticText *text = guienv->addStaticText(stringw(ddpi).c_str(),
		rect<s32>(5,5,635,35), false, false, 0, GUI_INFO_FPS );
	guienv->addEditBox( L"", rect<s32>(5,40,475,80));

	// add irrlicht logo
	IGUIImage * logo = guienv->addImage(driver->getTexture(internalStoragePath + "/media/irrlichtlogo3.png"),
					core::position2d<s32>(5,85), true, 0, GUI_IRR_LOGO);
	
	s32 minLogoWidth = 300;
	if ( logo && logo->getRelativePosition().getWidth() < minLogoWidth )
	{
		/* Scale to make it better visible on high-res devices (we could also work with dpi here).
		*/
		logo->setScaleImage(true);
		core::rect<s32> logoPos(logo->getRelativePosition());
		f32 scale = (f32)minLogoWidth/(f32)logoPos.getWidth();
		logoPos.LowerRightCorner.X = logoPos.UpperLeftCorner.X + minLogoWidth;
		logoPos.LowerRightCorner.Y = logoPos.UpperLeftCorner.Y + (s32)((f32)logoPos.getHeight()*scale);
		logo->setRelativePosition(logoPos);
	}

	/*
		Add a 3d model. Note that you might need to add light when using other models.
		A copy of the model and it's textures must be inside the assets folder to be installed to Android.
		In this example we do copy it to the assets folder in the Makefile jni/Android.mk
	*/
	IAnimatedMesh* mesh = smgr->getMesh(internalStoragePath + "/media/dwarf.x");
	if (!mesh)
	{
		device->closeDevice();
		device->drop();
	   	return 1;
	}
	smgr->addAnimatedMeshSceneNode( mesh );

	/*
	To look at the mesh, we place a camera.
	*/
	smgr->addCameraSceneNode(0, vector3df(15,40,-90), vector3df(0,30,0));

	/*
		Mainloop. Applications usually never quit themself in Android. The OS is responsible for that.
	*/
	mainloop(device, text);

	/* Cleanup */
	device->setEventReceiver(0);
	device->closeDevice();
	device->drop();
	
	return 0;
}

#endif	// defined(_IRR_ANDROID_PLATFORM_)

/*
**/

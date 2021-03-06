//  Copyright (c) 2016 Hugo Amiard hugo.amiard@laposte.net
//  This software is provided 'as-is' under the zlib License, see the LICENSE.txt file.
//  This notice and the license may not be removed or altered from any source distribution.

#include <toyui/Config.h>
#include <toyui/UiWindow.h>

#include <toyui/UiLayout.h>

#include <toyobj/String/String.h>
#include <toyobj/Util/Unique.h>

#include <toyui/Widget/Widget.h>
#include <toyui/Widget/Sheet.h>
#include <toyui/Widget/RootSheet.h>

#include <toyui/Frame/Frame.h>
#include <toyui/Frame/Stripe.h>

#include <toyui/Controller/Controller.h>

#include <stb_image.h>
#include <dirent.h>

namespace toy
{
	RenderSystem::RenderSystem(const string& resourcePath)
		: m_resourcePath(resourcePath)
	{}

	Context::Context(RenderSystem& renderSystem, unique_ptr<RenderWindow> renderWindow, unique_ptr<InputWindow> inputWindow)
		: m_renderSystem(renderSystem)
	{
		this->init(std::move(renderWindow), std::move(inputWindow));
	}

	Context::Context(RenderSystem& renderSystem)
		: m_renderSystem(renderSystem)
		, m_resourcePath(renderSystem.resourcePath())
	{}
	
	Context::~Context()
	{}

	void Context::init(unique_ptr<RenderWindow> renderWindow, unique_ptr<InputWindow> inputWindow)
	{
		m_renderWindow = std::move(renderWindow);
		m_inputWindow = std::move(inputWindow);
	}

	void spritesInFolder(std::vector<Image>& images, const string& path, const string& subfolder)
	{
		DIR* dir = opendir(path.c_str());
		dirent* ent;

		while((ent = readdir(dir)) != NULL)
			if(ent->d_type & DT_REG)
			{
				string fullpath = path + ent->d_name;
				string name = subfolder + replaceAll(ent->d_name, ".png", "");

				printf("Adding Image %s\n", fullpath.c_str());

				int width, height, n;
				unsigned char* img = stbi_load(fullpath.c_str(), &width, &height, &n, 4);
				stbi_image_free(img);

				images.emplace_back(name, fullpath, width, height);
			}
				

		closedir(dir);
	}

	UiWindow::UiWindow(RenderSystem& system, const string& name, int width, int height, bool fullScreen, User* user)
		: m_system(system)
		, m_resourcePath(system.resourcePath())
		, m_context(system.createContext(name, width, height, fullScreen))
		, m_renderer(system.createRenderer(*m_context))
		, m_images()
		, m_atlas(1024, 1024)
		, m_width(m_context->renderWindow().width())
		, m_height(m_context->renderWindow().height())
		, m_styler(make_unique<Styler>())
		, m_rootSheet(nullptr)
		, m_shutdownRequested(false)
		, m_user(user)
	{
		this->initResources();
		this->init();
	}

	UiWindow::~UiWindow()
	{
		for(Image& image : m_images)
			m_renderer->unloadImage(image);

		m_rootSheet->clear();
	}

	void UiWindow::init()
	{
		printf("UiWindow Init\n");
		m_renderer->setupContext();

		this->loadResources();

		m_styler->defaultLayout();

		m_rootSheet = make_unique<RootSheet>(*this);
		//m_rootDevice = make_unique<RootDevice>(*this, *m_rootSheet);

		m_context->inputWindow().initInput(m_rootSheet->mouse(), m_rootSheet->keyboard());

		m_rootSheet->frame().setSize(m_width, m_height);

		this->resize(size_t(m_width), size_t(m_height));

		printf("UiWindow Init End\n");
	}

	void UiWindow::initResources()
	{
		string spritePath = m_resourcePath + "interface/uisprites/";

		DIR* dir = opendir(spritePath.c_str());
		dirent* ent;

		spritesInFolder(m_images, spritePath, "");

		while((ent = readdir(dir)) != NULL)
			if(ent->d_type & DT_DIR && string(ent->d_name) != "." && string(ent->d_name) != "..")
				spritesInFolder(m_images, spritePath + ent->d_name + "/", string(ent->d_name) + "/");

		closedir(dir);
	}

	void UiWindow::loadResources()
	{
		m_renderer->loadFont();

		m_atlas.generateAtlas(m_images);

		for(Image& image : m_images)
			m_renderer->loadImage(image);

		m_renderer->loadImageRGBA(m_atlas.image(), m_atlas.data());
	}

	Image& UiWindow::createImage(const string& name, int width, int height, uint8_t* data)
	{
		m_images.emplace_back(name, name, width, height);
		Image& image = m_images.back();
		m_renderer->loadImageRGBA(image, data);
		return image;
	}

	void UiWindow::resize(size_t width, size_t height)
	{
		m_width = float(width);
		m_height = float(height);

		m_context->inputWindow().resize(width, height);

		m_rootSheet->frame().setSize(float(width), float(height));
	}

	bool UiWindow::nextFrame()
	{
		if(m_context->renderWindow().width() != size_t(m_width)
		|| m_context->renderWindow().height() != size_t(m_height))
			this->resize(m_context->renderWindow().width(), m_context->renderWindow().height());

		// if(manualRender)
		m_rootSheet->target().render();
		// add sub layers

		m_context->renderWindow().nextFrame();
		m_context->inputWindow().nextFrame();

		size_t tick = m_clock.readTick();
		size_t delta = m_clock.stepTick();

		m_rootSheet->nextFrame(tick, delta);

		return !m_shutdownRequested;
	}

	void UiWindow::shutdown()
	{
		m_shutdownRequested = true;
	}
}

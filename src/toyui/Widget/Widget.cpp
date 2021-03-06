//  Copyright (c) 2016 Hugo Amiard hugo.amiard@laposte.net
//  This software is provided 'as-is' under the zlib License, see the LICENSE.txt file.
//  This notice and the license may not be removed or altered from any source distribution.

#include <toyui/Config.h>
#include <toyui/Widget/Widget.h>

#include <toyui/Widget/Sheet.h>
#include <toyui/Widget/RootSheet.h>

#include <toyui/Frame/Frame.h>
#include <toyui/Frame/Stripe.h>
#include <toyui/Frame/Grid.h>
#include <toyui/Frame/Layer.h>

#include <toyui/UiLayout.h>
#include <toyui/UiWindow.h>

#include <toyobj/Iterable/Reverse.h>

namespace toy
{
	string Widget::sNullString;

	Widget::Widget(Wedge& parent, Type& type, FrameType frameType)
		: Widget(type, frameType, &parent)
	{
		parent.push(*this);
	}

	Widget::Widget(Type& type, FrameType frameType, Wedge* parent)
		: TypeObject(type)
		, m_parent(parent)
		, m_container(nullptr)
		, m_style(nullptr)
		, m_frame()
		, m_state(NOSTATE)
		, m_device(nullptr)
	{
		if(frameType == MASTER_LAYER)
			m_frame = make_unique<MasterLayer>(*this);
		else if(frameType == LAYER)
			m_frame = make_unique<Layer>(*this);
		else if(frameType == GRID)
			m_frame = make_unique<Grid>(*this);
		else if(frameType == TABLE)
			m_frame = make_unique<TableGrid>(*this);
		else if(frameType == MULTIGRID)
			m_frame = make_unique<MultiGrid>(*this);
		else if(frameType == STRIPE)
			m_frame = make_unique<Stripe>(*this);
		else if(frameType == FRAME)
			m_frame = make_unique<Frame>(*this);

		if(m_parent)
			this->updateStyle();
	}

	Widget::~Widget()
	{}

	RootSheet& Widget::rootSheet()
	{
		return m_parent->rootSheet();
	}

	UiWindow& Widget::uiWindow()
	{
		return this->rootSheet().uiWindow();
	}

	Context& Widget::context()
	{
		return this->uiWindow().context();
	}

	DrawFrame& Widget::content()
	{
		return m_frame->content();
	}

	const string& Widget::label()
	{
		return this->content().text();
	}

	void Widget::setLabel(const string& label)
	{
		this->content().setText(label);
	}

	Image* Widget::image()
	{
		return this->content().image();
	}

	void Widget::setImage(Image* image)
	{
		this->content().setImage(image);
	}

	const string& Widget::contentlabel()
	{
		return this->content().text();
	}

	void Widget::bind(Wedge& parent, size_t index, bool deferred)
	{
		m_parent = &parent;
		m_parentFrame = &parent;
		m_index = index;
		
		if(deferred)
			m_parent->frame().markDirty(Frame::DIRTY_MAPPING);
		else
			m_parent->stripe().map(*m_frame);

		RootSheet& rootSheet = this->rootSheet();
		this->visit([&rootSheet](Widget& widget) { rootSheet.handleBindWidget(widget); return true; });
	}

	void Widget::unbind()
	{
		RootSheet& rootSheet = this->rootSheet();
		this->visit([&rootSheet](Widget& widget) { rootSheet.handleUnbindWidget(widget); return true; });

		m_parent->stripe().unmap(*m_frame);

		m_parent = nullptr;
		m_parentFrame = nullptr;
		m_index = 0;
	}

	unique_ptr<Widget> Widget::extract()
	{
		unique_ptr<Widget> unique = m_container->release(*this);
		m_parent->destroy();
		return unique;
	}

	void Widget::remove()
	{
		m_container->release(*this);
	}

	void Widget::destroy()
	{
		m_container->release(*this);
	}

	Widget* Widget::findContainer(Type& type)
	{
		Widget* widget = this->parent();

		while(widget)
		{
			if(&widget->type() == &type)
				return widget;
			widget = widget->parent();
		}

		return nullptr;
	}

	void Widget::visit(const Visitor& visitor)
	{
		visitor(*this);
	}

	void Widget::nextFrame(size_t tick, size_t step)
	{
		if(m_frame->dirty())
			m_frame->layer().setRedraw();

		m_frame->clearDirty();

		if(m_style->updated() > m_frame->styleStamp())
			m_frame->resetStyle();
	}

	void Widget::render(Renderer& renderer, bool force)
	{
		m_frame->content().beginDraw(renderer, force);
		m_frame->content().draw(renderer, force);
		m_frame->content().endDraw(renderer);
	}

	void Widget::show()
	{
		m_frame->show();
	}
	
	void Widget::hide()
	{
		m_frame->hide();
	}

	void Widget::updateStyle()
	{
		m_style = &this->fetchStyle(m_type);
		m_frame->setStyle(*m_style);
	}

	void Widget::setStyle(Style& style, bool hard)
	{
		m_style = &style;
		m_frame->setStyle(*m_style, hard);
	}

	void Widget::setStyle(Type& type, bool hard)
	{
		this->setStyle(this->fetchStyle(type), hard);
	}

	Style& Widget::fetchStyle(Type& type)
	{
		return this->uiWindow().styler().style(type);
	}

	void Widget::markDirty()
	{
		m_frame->setDirty(Frame::DIRTY_CONTENT);
	}

	void Widget::toggleState(WidgetState state)
	{
		m_state = static_cast<WidgetState>(m_state ^ state);
		this->updateState();
	}

	void Widget::enableState(WidgetState state)
	{
		if(!(m_state & state))
			this->toggleState(state);
	}

	void Widget::disableState(WidgetState state)
	{
		if(m_state & state)
			this->toggleState(state);
	}

	void Widget::updateState()
	{
		m_frame->content().updateInkstyle(m_style->subskin(m_state));
		m_frame->setDirty(Frame::DIRTY_CONTENT);
	}

	Widget* Widget::pinpoint(float x, float y)
	{
		if(m_frame->dirty() >= Frame::DIRTY_MAPPING)
			return nullptr;
		DimFloat absolute = m_frame->absolutePosition();
		Frame* frame = m_frame->pinpoint(x - absolute[DIM_X], y - absolute[DIM_Y], true);
		return frame ? frame->widget() : nullptr;
	}

	InputReceiver* Widget::controlEvent(InputEvent& inputEvent)
	{
		if(m_controller && m_controller->consumes(inputEvent.deviceType))
			return m_controller->controlEvent(inputEvent);

		if(inputEvent.deviceType >= InputEvent::DEVICE_MOUSE && m_controlMode < CM_ABSOLUTE)
		{
			MouseEvent& mouseEvent = static_cast<MouseEvent&>(inputEvent);
			Widget* pinned = this->pinpoint(mouseEvent.posX, mouseEvent.posY);
			return pinned ? pinned : this;
		}

		return this;
	}

	InputReceiver* Widget::propagateEvent(InputEvent& inputEvent)
	{
		UNUSED(inputEvent);
		return m_parent;
	}

	void Widget::activate()
	{
		this->enableState(ACTIVATED);
	}

	void Widget::deactivate()
	{
		this->disableState(ACTIVATED);
	}
	
	void Widget::modal()
	{
		this->enableState(MODAL);
	}

	void Widget::unmodal()
	{
		this->disableState(MODAL);
	}

	void Widget::control()
	{
		this->enableState(CONTROL);
		this->enableState(FOCUSED);
		this->focused();
	}

	void Widget::uncontrol()
	{
		this->disableState(CONTROL);
		this->disableState(FOCUSED);
		this->unfocused();
	}

	void Widget::mouseEntered(MouseEvent& mouseEvent)
	{
		UNUSED(mouseEvent);
		this->enableState(HOVERED);
	}

	void Widget::mouseLeaved(MouseEvent& mouseEvent)
	{
		UNUSED(mouseEvent);
		this->disableState(HOVERED);
	}

	void Widget::mousePressed(MouseEvent& mouseEvent)
	{
		UNUSED(mouseEvent);
		this->enableState(PRESSED);
	}

	void Widget::mouseReleased(MouseEvent& mouseEvent)
	{
		UNUSED(mouseEvent);
		this->disableState(PRESSED);
	}

	InputReceiver* Widget::receiveEvent(InputEvent& inputEvent)
	{
		if(inputEvent.consumed)
			return this;

		inputEvent.visited.push_back(this);

		if(inputEvent.deviceType >= InputEvent::DEVICE_MOUSE)
		{
			MouseEvent& mouseEvent = static_cast<MouseEvent&>(inputEvent);
			/*DimFloat absolute = m_frame->absolutePosition();
			mouseEvent.relativeX = (mouseEvent.posX - absolute[DIM_X]) / m_frame->scale();
			mouseEvent.relativeY = (mouseEvent.posY - absolute[DIM_Y]) / m_frame->scale();*/
			DimFloat local = m_frame->localPosition(mouseEvent.posX, mouseEvent.posY);
			mouseEvent.relativeX = local.x();
			mouseEvent.relativeY = local.y();
		}

		return InputWidget::receiveEvent(inputEvent);
	}

	Item::Item(Wedge& parent, Type& type)
		: Widget(parent, type)
	{}

	Control::Control(Wedge& parent, Type& type)
		: Item(parent, type)
	{}
}

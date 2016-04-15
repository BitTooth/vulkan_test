#include <iostream>
#include <vector>
#include <cassert>
#include <Windows.h>

#define VK_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR

#include "../vulkan_sdk/include/vulkan.h"
#include "../vulkan_sdk/include/vk_sdk_platform.h"
#pragma comment(lib, "../vulkan_sdk/lib/vulkan-1.lib")

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Defines
//

typedef				__int8		i8;
typedef				__int16		i16;
typedef				__int32		i32;
typedef				__int64		i64;
typedef unsigned	__int8		u8;
typedef unsigned	__int16		u16;
typedef unsigned	__int32		u32;
typedef unsigned	__int64		u64;

// Vulkan automated errors checking
#define HR(x) {																\
	VkResult res = (x);														\
	if (res != VK_SUCCESS)													\
	{																		\
		std::cout	<< "vulkan api method call failed with error " << res	\
					<< " file: " << __FILE__								\
					<< " line: " << __LINE__								\
					<<std::endl;											\
	}																		\
}

// Vulkan related structs

struct SwapChainBuffer
{
	VkImage			image;
	VkImageView		view;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Globals
//

// WinAPI stuff
HWND									ghWnd;
HINSTANCE								ghInstance;
bool									gClose = false;
u32										gWidth = 640;
u32										gHeight = 480;

// Vulkan stuff
VkInstance								gInstance;			// Like Direct3D instance
VkPhysicalDevice						gDevices[1];		// Just list of videoadapters presented in system
u32										gDeviceCount = 0;	// Count of physical videadapters in the system

VkDevice								gDevice;			// Vulkan logical device as D3D11Device

// queue info
std::vector<VkQueueFamilyProperties>	gQueueProps;		// What is queue in vulkan???
u32										gQueueCount = 0;	// Number of queues of device
u32										gQueueFamilyIndex = -1;	// Selected queue index
VkDeviceQueueCreateInfo					gQueueInfo = {};	// Info about selected queue
float									gQueuePriorities[1] = { 1.0f };
VkQueue									gQueue;

// command buffers for selected queue
VkCommandPool							gCmdPool;			// OK, it looks like command buffers in DirectX 11
VkCommandBuffer							gCmd;				// ...

// swapchaing info
VkSurfaceKHR							gSurface;			// surface selected for drawing
VkSurfaceCapabilitiesKHR				gSurfaceCaps;		// selected surface capabilities
std::vector<VkPresentModeKHR>			gPresentModes;		// surface supported present modes
VkPresentModeKHR						gPresentMode;		// selected present mode
VkSwapchainKHR							gSwapchain;			// main swapchain
std::vector<SwapChainBuffer>			gSwapBuffers;		// images used in swapchain

// surface formats
std::vector<VkSurfaceFormatKHR>			gFormates;			// supported surface formates
VkFormat								gFormat;			// selected format




/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WinAPI
//
LRESULT CALLBACK wndCallback( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	switch( msg )
	{
	case WM_CLOSE:
		DestroyWindow( hwnd );
		gClose = true;
		break;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}

bool createWindow()
{
	std::cout << "creating window...";
	ghInstance = GetModuleHandle( NULL );
	WNDCLASS wndclass = {};
	wndclass.lpszClassName	= "vulkan_test";
	wndclass.cbClsExtra		= 0;
	wndclass.style			= CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc	= &wndCallback;
	wndclass.cbWndExtra		= 0;
	wndclass.hbrBackground	= (HBRUSH)COLOR_WINDOW;
	wndclass.hCursor		= NULL;
	wndclass.hIcon			= NULL;
	wndclass.hInstance		= ghInstance;
	wndclass.lpszMenuName	= NULL;

	if( !RegisterClass( &wndclass ) )
	{
		std::cout << "error registering window class" << std::endl;
		return false;
	}

	ghWnd = CreateWindow("vulkan_test", "Vulkan Test", WS_OVERLAPPEDWINDOW,	CW_USEDEFAULT, 
						  CW_USEDEFAULT, gWidth, gHeight, NULL, NULL, ghInstance, NULL);
	if( !ghWnd )
	{
		std::cout << "error creating window\n";
		return false;
	}

	ShowWindow( ghWnd, false );

	std::cout << "window created\n";
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Vulkan init
//
bool initVkInstance( const char* appName, const char* engineName )
{
	std::cout << "\nTrying to init vulkan API\n";

	std::vector<const char*> extensions;
	std::vector<const char*> layers;

	extensions.push_back( VK_KHR_SURFACE_EXTENSION_NAME );
	extensions.push_back( VK_KHR_WIN32_SURFACE_EXTENSION_NAME );

	VkApplicationInfo appInfo = {};
	appInfo.apiVersion = VK_API_VERSION_1_0;
	appInfo.applicationVersion = 1;
	appInfo.engineVersion = 1;
	appInfo.pApplicationName = appName;
	appInfo.pEngineName = engineName;
	appInfo.pNext = nullptr;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;


	VkInstanceCreateInfo instInfo = {};
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pNext = nullptr;
	instInfo.pApplicationInfo = &appInfo;
	instInfo.flags = 0;
	instInfo.enabledExtensionCount = extensions.size();
	instInfo.ppEnabledExtensionNames = extensions.size() ? extensions.data() : nullptr;
	instInfo.enabledLayerCount = layers.size();
	instInfo.ppEnabledLayerNames = layers.size() ? layers.data() : nullptr;

	VkResult res;

	res = vkCreateInstance( &instInfo, nullptr, &gInstance );
	if( res == VK_ERROR_INCOMPATIBLE_DRIVER )
	{
		std::cout << "Incompatible driver\n";
	}
	else if( res )
	{
		std::cout << "Unknown error\n";
	}
	else
	{
		std::cout << "Instance inited\n";
		return true;
	}
	return false;
}

void getDevicesList()
{
	vkEnumeratePhysicalDevices( gInstance, &gDeviceCount, gDevices );

	std::cout << "Device list:\n";
	for( u32 i = 0; i < gDeviceCount; ++i )
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties( gDevices[i], &properties );
		std::cout << "\t" << properties.deviceName << std::endl;
	}
}

bool findSupportedQueue()
{
	std::cout << "looking for supported queue...";
	
	vkGetPhysicalDeviceQueueFamilyProperties( gDevices[0], &gQueueCount, nullptr );

	gQueueProps.resize( gQueueCount );
	vkGetPhysicalDeviceQueueFamilyProperties( gDevices[0], &gQueueCount, gQueueProps.data() );

	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = nullptr;
	createInfo.hinstance = ghInstance;
	createInfo.hwnd = ghWnd;
	createInfo.flags = 0;

	gQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	gQueueInfo.pNext = nullptr;
	gQueueInfo.queueCount = 1;
	gQueueInfo.pQueuePriorities = gQueuePriorities;

	VkResult res = vkCreateWin32SurfaceKHR( gInstance, &createInfo, nullptr, &gSurface );

	if( res != VK_SUCCESS )
	{
		std::cout << "surface creating error " << res << std::endl;
		return false;
	}

	gQueueFamilyIndex = -1;
	for( u32 i = 0; i < gQueueCount; ++i )
	{
		if( gQueueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )
		{
			VkBool32 support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR( gDevices[0], i, gSurface, &support );
			if( support == VK_TRUE )
			{
				gQueueFamilyIndex = i;
				break;
			}
		}
	}
	if( gQueueFamilyIndex == -1 )
	{
		std::cout << "supported queue not found\n" << std::endl;
		return false;
	}

	gQueueInfo.queueFamilyIndex = gQueueFamilyIndex;
	
	std::cout << "found\n";
	return true;
}

bool createDevice()
{
	std::cout << "creating vulkan device...";

	std::vector<const char*> extensions;
	std::vector<const char*> layers;

	extensions.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext = nullptr;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &gQueueInfo;
	deviceInfo.enabledExtensionCount = extensions.size();
	deviceInfo.ppEnabledExtensionNames = extensions.size() ? extensions.data() : nullptr;
	deviceInfo.enabledLayerCount = layers.size();
	deviceInfo.ppEnabledLayerNames = layers.size() ? layers.data() : nullptr;
	deviceInfo.pEnabledFeatures = nullptr;

	VkResult res = vkCreateDevice( gDevices[0], &deviceInfo, nullptr, &gDevice );
	if( res != VK_SUCCESS )
	{
		std::cout << "vulkan device create error " << res <<std::endl;
		return false;
	}
	
	std::cout << "device created\n";
	return true;
}
 
bool initCommandBuffers()
{
	std::cout << "creating command buffers...";
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.pNext = nullptr;
	cmdPoolInfo.queueFamilyIndex = gQueueFamilyIndex;
	cmdPoolInfo.flags = 0;

	VkResult res = vkCreateCommandPool( gDevice, &cmdPoolInfo, nullptr, &gCmdPool );
	if( res != VK_SUCCESS )
	{
		std::cout << "error creating command pool\n";
		return false;
	}
	else
	{
		VkCommandBufferAllocateInfo cmdInfo = {};
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdInfo.pNext = nullptr;
		cmdInfo.commandPool = gCmdPool;
		cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdInfo.commandBufferCount = 1;

		res = vkAllocateCommandBuffers( gDevice, &cmdInfo, &gCmd );
		if( res != VK_SUCCESS )
		{
			std::cout << "error allocating command buffer\n";
			vkDestroyCommandPool( gDevice, gCmdPool, nullptr );
			return false;
		}
	}

	std::cout << "command buffer created\n";
	return true;
}

bool getSurfaceFormats()
{
	VkResult res;
	u32 formatCount = 0;
	res = vkGetPhysicalDeviceSurfaceFormatsKHR( gDevices[0], gSurface, &formatCount, nullptr );
	if( res == VK_SUCCESS )
	{
		gFormates.resize( formatCount );
		res = vkGetPhysicalDeviceSurfaceFormatsKHR( gDevices[0], gSurface, &formatCount, gFormates.data() );
		if( res != VK_SUCCESS )
		{
			std::cout << "getting surface formats failed\n";
			return false;
		}

		if( formatCount == 1 && gFormates[0].format == VK_FORMAT_UNDEFINED )
		{
			gFormat = VK_FORMAT_B8G8R8A8_UNORM;
		}
		else
		{
			gFormat = gFormates[0].format;
		}
	}
	else
	{
		std::cout << "getting surface formats failed\n";
		return false;
	}
	return true;
}

bool getSurfacePresentModes()
{
	u32 modesCount = 0;
	HR( vkGetPhysicalDeviceSurfacePresentModesKHR( gDevices[0], gSurface, &modesCount, nullptr ) );
	gPresentModes.resize( modesCount );
	HR( vkGetPhysicalDeviceSurfacePresentModesKHR( gDevices[0], gSurface, &modesCount, gPresentModes.data() ) );
	return true;
}

bool setImageLayout( VkCommandBuffer cmdBuf, VkImage image, VkImageAspectFlags aspects, VkImageLayout oldLayout, VkImageLayout newLayout )
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspects;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	switch( oldLayout )
	{
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
	}

	switch( newLayout )
	{
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			barrier.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			barrier.dstAccessMask |=
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			barrier.srcAccessMask =
				VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
	}

	vkCmdPipelineBarrier( cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
						  0, 0, nullptr, 0, nullptr, 1, &barrier );

	return true;
}

void beginCommandBuffer( VkCommandBuffer cmdBuf )
{
	VkCommandBufferBeginInfo cmd = {};
	cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmd.pNext = nullptr;
	cmd.flags = 0;
	cmd.pInheritanceInfo = 0;

	HR( vkBeginCommandBuffer( cmdBuf, &cmd ) );
}

void endCommandBuffer( VkCommandBuffer cmdBuf )
{
	HR( vkEndCommandBuffer( cmdBuf ) );
}

void executeQueue( VkCommandBuffer cmd )
{
	const VkCommandBuffer cmds[] = { cmd };
	VkFenceCreateInfo fenceInfo;
	VkFence drawFence;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = 0;
	vkCreateFence( gDevice, &fenceInfo, nullptr, &drawFence );

	VkPipelineStageFlags pipeStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo submitInfo[1] = {};
	submitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo[0].pNext = nullptr;
	submitInfo[0].waitSemaphoreCount = 0;
	submitInfo[0].pWaitSemaphores = nullptr;
	submitInfo[0].pWaitDstStageMask = &pipeStageFlags;
	submitInfo[0].commandBufferCount = 1;
	submitInfo[0].pCommandBuffers = cmds;
	submitInfo[0].signalSemaphoreCount = 0;
	submitInfo[0].pSignalSemaphores = nullptr;

	HR( vkQueueSubmit( gQueue, 1, submitInfo, drawFence ) );

	VkResult res;
	do {
		res = vkWaitForFences( gDevice, 1, &drawFence, VK_TRUE, 100000000 );
	} while( res == VK_TIMEOUT );

	vkDestroyFence( gDevice, drawFence, nullptr );
}

bool initSwapChains()
{
	std::cout << "initing swapchain...";
	if( !getSurfaceFormats() || !getSurfacePresentModes() )
	{
		return false;
	}

	VkResult res;
	res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR( gDevices[0], gSurface, &gSurfaceCaps );
	if( res != VK_SUCCESS )
	{
		std::cout << "error getting surface capabilities\n";
	}

	VkExtent2D swapChainExtent = gSurfaceCaps.currentExtent;
	if( std::find( gPresentModes.begin(), gPresentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR ) != gPresentModes.end() )
		gPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	else if( std::find( gPresentModes.begin(), gPresentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR ) != gPresentModes.end() )
		gPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	else
		gPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	uint32_t desiredNumberOfSwapChainImages = gSurfaceCaps.minImageCount + 1;
	desiredNumberOfSwapChainImages = gSurfaceCaps.maxImageCount ? max( desiredNumberOfSwapChainImages, gSurfaceCaps.maxImageCount ) : desiredNumberOfSwapChainImages;

	VkSurfaceTransformFlagBitsKHR preTransform;
	preTransform = gSurfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : gSurfaceCaps.currentTransform;

	VkSwapchainCreateInfoKHR swapChain = {};
	swapChain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChain.pNext = nullptr;
	swapChain.surface = gSurface;
	swapChain.minImageCount = desiredNumberOfSwapChainImages;
	swapChain.imageFormat = gFormat;
	swapChain.imageExtent = swapChainExtent;
	swapChain.preTransform = preTransform;
	swapChain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChain.imageArrayLayers = 1;
	swapChain.presentMode = gPresentMode;
	swapChain.oldSwapchain = NULL;
	swapChain.clipped = true;
	swapChain.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	swapChain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChain.queueFamilyIndexCount = 0;
	swapChain.pQueueFamilyIndices = nullptr;

	res = vkCreateSwapchainKHR( gDevice, &swapChain, nullptr, &gSwapchain );
	if( res != VK_SUCCESS )
	{
		std::cout << "error creating swapchain "<< res << std::endl;
		return false;
	}

	std::vector<VkImage> images;
	u32 imagesCount = 0;
	HR( vkGetSwapchainImagesKHR(gDevice, gSwapchain, &imagesCount, nullptr ) );
	images.resize( imagesCount );
	HR( vkGetSwapchainImagesKHR(gDevice, gSwapchain, &imagesCount, images.data() ) );

	beginCommandBuffer( gCmd );
	vkGetDeviceQueue( gDevice, gQueueFamilyIndex, 0, &gQueue );

	gSwapBuffers.resize( imagesCount );
	for( u32 i = 0; i < gSwapBuffers.size(); ++i )
	{
		VkImageViewCreateInfo imageView = {};
		imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageView.pNext = nullptr;
		imageView.format = gFormat;
		imageView.components.r = VK_COMPONENT_SWIZZLE_R;
		imageView.components.g = VK_COMPONENT_SWIZZLE_G;
		imageView.components.b = VK_COMPONENT_SWIZZLE_B;
		imageView.components.a = VK_COMPONENT_SWIZZLE_A;
		imageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.flags = 0;
		imageView.image = images[i];

		setImageLayout( gCmd, gSwapBuffers[i].image, VK_IMAGE_ASPECT_COLOR_BIT, 
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
		HR( vkCreateImageView( gDevice, &imageView, nullptr, &gSwapBuffers[i].view ) );
	}

	endCommandBuffer( gCmd );
	executeQueue( gCmd );

	std::cout << "inited\n";
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main
//
int main()
{
	if( createWindow() )
	{
		if( initVkInstance( "vulkan_test", "lamp_engine" ) )
		{
			getDevicesList();

			if( findSupportedQueue() && createDevice() )
			{
				if( initCommandBuffers() )
				{
					if( initSwapChains() )
					{
						ShowWindow( ghWnd, true );

						// Start loop
						MSG msg;
						while( !gClose && GetMessage( &msg, NULL, 0, 0 ) > 0 )
						{
							TranslateMessage( &msg );
							DispatchMessage( &msg );
						}
					}

					vkFreeCommandBuffers( gDevice, gCmdPool, 1, &gCmd );
					vkDestroyCommandPool( gDevice, gCmdPool, nullptr );
				}

				vkDestroyDevice( gDevice, nullptr );
			}
			vkDestroyInstance( gInstance, nullptr );
		}
	}

	system("PAUSE");

	return 0;
}
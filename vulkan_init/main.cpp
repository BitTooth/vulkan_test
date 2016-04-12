#include <iostream>

#include "../vulkan_sdk/include/vulkan.h"
#pragma comment(lib, "../vulkan_sdk/lib/vulkan-1.lib")

VkInstance gInstance;

bool initVkInstance( const char* appName, const char* engineName )
{
	std::cout << "Trying to init vulkan API\n";

	VkApplicationInfo appInfo = {};
	appInfo.apiVersion = VK_API_VERSION_1_0;
	appInfo.applicationVersion = 1;
	appInfo.engineVersion = 1;
	appInfo.pApplicationName = appName;
	appInfo.pEngineName = engineName;
	appInfo.pNext = nullptr;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

	VkInstanceCreateInfo instInfo = {};
	instInfo.enabledExtensionCount = 0;
	instInfo.enabledLayerCount = 0;
	instInfo.flags = 0;
	instInfo.pApplicationInfo = &appInfo;
	instInfo.pNext = nullptr;
	instInfo.ppEnabledExtensionNames = nullptr;
	instInfo.ppEnabledLayerNames = nullptr;
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

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
	}
}

int main()
{
	initVkInstance( "vulkan_test", "lamp_engine" );


	system("PAUSE");

	vkDestroyInstance( gInstance, nullptr );

	return 0;
}
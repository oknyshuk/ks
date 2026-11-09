#pragma once

inline constexpr const char *BLACKBOX_INTERFACE_VERSION = "BlackBoxVersion001";

class IBlackBox 
{
public:
	void Record(int type, const char *fmt);
	void SetLimit(int type, unsigned int count);
	const char *Get(int type, unsigned int index);
	int Count(int type);
	void Flush(int type);

	const char *GetTypeName(int type);
	int GetTypeCount();
};


#include "GD.h"

namespace Bose
{
	BaseLib::SharedObjects* GD::bl = nullptr;
	Bose* GD::family = nullptr;
	std::string GD::dataPath;
	std::shared_ptr<IBoseInterface> GD::physicalInterface;
	BaseLib::Output GD::out;
}

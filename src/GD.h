#ifndef GD_H_
#define GD_H_

#define Bose_FAMILY_ID 66
#define Bose_FAMILY_NAME "Bose"

#include <homegear-base/BaseLib.h>
#include "Bose.h"
#include "PhysicalInterfaces/IBoseInterface.h"

namespace Bose
{

class GD
{
public:
	virtual ~GD();

	static BaseLib::SharedObjects* bl;
	static Bose* family;
	static std::string dataPath;
	static std::shared_ptr<IBoseInterface> physicalInterface;
	static BaseLib::Output out;
private:
	GD();
};

}

#endif /* GD_H_ */

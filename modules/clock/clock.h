#include <module_base.h>

class Clock : public TBModule {
public:
	const char* Name() const override;

	void Draw(IDirect3DDevice9* device);
};


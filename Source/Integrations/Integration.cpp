#include "Integration.hpp"

#include "../CApp.h"

Integration::Integration(CApp *app) : _app(app) {
	_app->AddIntegration(this);
}

Integration::~Integration() {
	
}

void Integration::OnDestroy() {
	_app->RemoveIntegration(this);
}
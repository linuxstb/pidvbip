/*

pidvbip - tvheadend client for the Raspberry Pi

(C) Dave Chapman 2012-2013
(C) Daniel Nordqvist 2013

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "osd_model.h"
#include "events.h"

void setModelChannelList(model_channellist_t *model, int index, int id, int lcn, char *name, int selected)
{
  printf("Enter setModelChannelList: %d %d %d %s %d\n", index, id, lcn, name, selected);
  if (selected) {
    model->selectedIndex = index;
  }
  
  model->channel[index].id = id;
  model->channel[index].lcn = lcn; 
  snprintf(model->channel[index].name, sizeof(model->channel[index].name), "%s", name); 
}

void clearModelChannelList(model_channellist_t *model) 
{
  int i;
  memset(model, 0, sizeof(model_channellist_t));
  
  for (i = 0; i < CHANNELLIST_NUM_CHANNELS; i++) {
    model->channel[i].id = -1;
  }  
}

void copyModelChannelList(model_channellist_t *toModel, const model_channellist_t *fromModel)
{
  memcpy(toModel, fromModel, sizeof(model_channellist_t));
}

void setSelectedIndex(model_channellist_t *model, int index) {
  model->selectedIndex = index;
}

int compareIndexModelChannelList(model_channellist_t *newModel, model_channellist_t *oldModel, int index)
{
  if ((newModel->selectedIndex == index || oldModel->selectedIndex == index) ||
      newModel->channel[index].id != oldModel->channel[index].id ||
      (oldModel->channel[index].id == -1 && newModel->channel[index].id != -1) ||
      (oldModel->selectedIndex == index && oldModel->active != newModel->active )) {
          return 1;
  }
  return 0;
}

void compareModelChannelList(model_channellist_t *newModel, model_channellist_t *oldModel, void *fkn)
{
}

void setModelNowNext(model_now_next_t *model, uint32_t nowEvent, uint32_t nextEvent, int server)
{  
  if (model->nowEvent != NULL) {
    event_free(model->nowEvent);
  }

  if (model->nextEvent != NULL) {
    event_free(model->nextEvent);
  }
  
  model->nowEvent = event_copy(nowEvent, server);
  model->nextEvent = event_copy(nextEvent, server);
  model->selectedIndex = 0;
}




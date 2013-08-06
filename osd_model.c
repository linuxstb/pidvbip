#include "osd_model.h"
#include "events.h"

void setModelChannelList(model_channellist_t *model, int index, int id, int lcn, char *name, int selected)
{
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




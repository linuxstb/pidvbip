#include "osd_model.h"

#define NUM_OF_CHANNELS 12

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
  
  for (i = 0; i < NUM_OF_CHANNELS; i++) {
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
      (oldModel->channel[index].id == -1 && newModel->channel[index].id != -1)) {
          return 1;
  }
  return 0;
}

void compareModelChannelList(model_channellist_t *newModel, model_channellist_t *oldModel, void *fkn)
{
}

#if 0
void main()
{
  model_channellist_t model_channellist;
  model_channellist_t model_channellist_new;
  clearModelChannelList(&model_channellist);
  clearModelChannelList(&model_channellist_new);
  
  setModelChannelList(&model_channellist, 0, 0, 0, "kanal 0", 1);
  setModelChannelList(&model_channellist, 1, 1, 1, "kanal 1", 0);
  setModelChannelList(&model_channellist, 2, 2, 2, "kanal 2", 0);
  
  copyModelChannelList(&model_channellist_new, &model_channellist);
  
  int i;
  for (i = 0; i < NUM_OF_CHANNELS; i++) {
    if (model_channellist_new.channel[i].id != -1) {
      printf("%d %d %s\n", model_channellist.channel[i].id, model_channellist.channel[i].lcn, model_channellist.channel[i].name);
    }  
  }  
}

#endif
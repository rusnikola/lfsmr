# Copyright 2015 University of Rochester
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. 

###############################################################
### This script generates the 8 plots that were actually    ###
### used in the paper using the data contained in ../final/ ###
###############################################################

library(plyr)
library(ggplot2)

filenames<-c("bonsai", "hashmap","natarajan","list")
for (f in filenames){
read.csv(paste("../final/",f,"_result_retired_read.csv",sep=""))->lindata

lindata$environment<-as.factor(gsub("emptyf=120:tracker=RCU","Epoch",lindata$environment))
lindata$environment<-as.factor(gsub("emptyf=120:tracker=HE","HE",lindata$environment))
lindata$environment<-as.factor(gsub("emptyf=120:tracker=Hazard","HP",lindata$environment))
lindata$environment<-as.factor(gsub("emptyf=120:tracker=HyalineOEL","Hyaline-1",lindata$environment))
lindata$environment<-as.factor(gsub("emptyf=120:tracker=HyalineEL","Hyaline",lindata$environment))
lindata$environment<-as.factor(gsub("emptyf=120:tracker=HyalineOSEL","Hyaline-1S",lindata$environment))
lindata$environment<-as.factor(gsub("emptyf=120:tracker=HyalineSEL","Hyaline-S",lindata$environment))
lindata$environment<-as.factor(gsub("emptyf=120:tracker=Range_new","IBR",lindata$environment))

# Compute average and max retired objects per operation from raw data
ddply(.data=lindata,.(environment,threads),mutate,retired_avg= mean(obj_retired)/(mean(ops)))->lindata
ddply(.data=lindata,.(environment,threads),mutate,ops_max= max(ops)/(interval*1000000))->lindata

rcudatalin <- subset(lindata,environment=="Epoch")
hazarddatalin <- subset(lindata,environment=="HP")
hedatalin <- subset(lindata,environment=="HE")
chaindatalin <- subset(lindata,environment=="Hyaline")
chainodatalin <- subset(lindata,environment=="Hyaline-1")
gtchaindatalin <- subset(lindata,environment=="Hyaline-S")
gtchainodatalin <- subset(lindata,environment=="Hyaline-1S")
rangenewdatalin <- subset(lindata,environment=="IBR")

lindata = rbind(rcudatalin, rangenewdatalin, chaindatalin, chainodatalin, gtchaindatalin, gtchainodatalin, hedatalin, hazarddatalin)
lindata$environment <- factor(lindata$environment, levels=c("Epoch", "IBR", "Hyaline", "Hyaline-1", "Hyaline-S", "Hyaline-1S", "HE", "HP"))

# Set up colors and shapes (invariant for all plots)
color_key = c("#0000FF", "#0066FF", "#FF0000", "#FF007F",
              "#013220", "#3EB489", "#800080", "#FFAA1D")
names(color_key) <- unique(c(as.character(lindata$environment)))

shape_key = c(5,0,6,4,1,3,2,62)
names(shape_key) <- unique(c(as.character(lindata$environment)))

line_key = c(1,4,1,2,1,2,4,1)
names(line_key) <- unique(c(as.character(lindata$environment)))

##########################################
#### Begin charts for retired objects ####
##########################################

legend_pos=c(0.45,0.9)
y_range_down = 0
y_range_up = 2000

# Benchmark-specific plot formatting
if(f=="bonsai"){
  y_range_down=0
  legend_pos=c(0.45,0.9)
  y_range_up=2200
}else if(f=="list"){
  y_range_down=0
  y_range_up=420
}else if(f=="hashmap"){
  y_range_up=4500
}else if(f=="natarajan"){
  y_range_up=4500
}

# Generate the plots
linchart<-ggplot(data=lindata,
                  aes(x=threads,y=retired_avg,color=environment, shape=environment, linetype=environment))+
  geom_line()+xlab("Threads")+ylab("Retired Objects per Operation")+geom_point(size=4)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$environment])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$environment])+
  theme_bw()+ guides(shape=guide_legend(title=NULL,nrow = 2))+ 
  guides(color=guide_legend(title=NULL,nrow = 2))+
  guides(linetype=guide_legend(title=NULL,nrow = 2))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$environment])+
  scale_x_continuous(breaks=c(1,9,18,27,36,45,54,63,72,81,90,99,108,117,126,135,144),
                minor_breaks=c(1,9,18,27,36,45,54,63,72,81,90,99,108,117,126,135,144))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  theme(legend.position=legend_pos,
     legend.direction="horizontal")+
  theme(text = element_text(size = 20))+
  theme(axis.title.y = element_text(margin = margin(t = 0, r = 5, b = 0, l = 5)))+
  theme(axis.title.x = element_text(margin = margin(t = 5, r = 0, b = 5, l = 0)))+
  theme(panel.border = element_rect(size = 1.5))+
  ylim(y_range_down,y_range_up)

# Save all four plots to separate PDFs
ggsave(filename = paste("../final/",f,"_linchart_retired_read.pdf",sep=""),linchart,width=7.3, height = 5, units = "in", dpi=300)

}

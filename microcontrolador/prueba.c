        if(action.equals("sendState")){
           Serial.println("Recibir y almacenar estado de la nevera");
        }
        if(action.equals("confirmConnection")){
          Serial.println("La conexion de la nevera fue verificada y se actualiza los datos");
        }
        if(action.equals("error")){
          Serial.println("Se recibio un error de la nevera");
        }
        if(action.equals("setTemperature")){
          Serial.println("Hay que indicarle a la nevera seleccionada la temperatura que se indico");
        }
        if(action.equals("setTemperatureForAll")){
          Serial.println("Indicarle a todas las neveras que deben ponerse a la temperatura indicada");
        }
        if(action.equals("toggleLight")){
          Serial.println("Indicarle a la nevera seleccionada que prenda la luz");
        }
        if(action.equals("setMaxTemperature")){
          Serial.println("Indicarle a la nevera seleccionada que cambie su nivel maximo de temperature");
        }
        if(action.equals("setMinTemperature")){
          Serial.println("Indicarle a la nevera seleccionada que cambie su nivel minimo de temperature");
        }
        if(action.equals("setMaxTemperatureForAll")){
          Serial.println("Indicarle a todas las neveras que cambien su nivel maximo de temperature");
        }
        if(action.equals("setMinTemperatureForAll")){
          Serial.println("Indicarle a todas las neveras que cambien su nivel minimo de temperature");
        }
        if(action.equals("delete")){
          Serial.println("Eliminar la nevera indicada");
        }
        if(action.equals("deleteAll")){
          Serial.println("Eliminar todas las neveras");
        }
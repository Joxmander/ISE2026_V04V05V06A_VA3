t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <title>Panel REACT - Nodo A</title>
t <style>
t   body { font-family: 'Segoe UI', Tahoma, Geneva, sans-serif; 
t          background-color: #eef2f5; color: #333; margin: 0; padding: 20px; }
t   .header { text-align: center; margin-bottom: 30px; }
t   .logo { max-width: 200px; border-radius: 10px; 
t           box-shadow: 0 4px 8px rgba(0,0,0,0.2); margin-bottom: 15px; }
t   .container { max-width: 800px; margin: 0 auto; background: #fff; 
t                padding: 30px; border-radius: 12px; 
t                box-shadow: 0 5px 15px rgba(0,0,0,0.1); }
t   h1 { color: #2c3e50; text-align: center; margin: 0; font-size: 2em; }
t   h2 { color: #34495e; border-bottom: 2px solid #3498db; 
t        padding-bottom: 5px; margin-top: 30px; }
t   .info-box { background-color: #f8f9fa; border-left: 5px solid #3498db; 
t               padding: 15px; margin-bottom: 20px; font-size: 1.1em; }
t   .form-group { margin-bottom: 20px; }
t   select, input[type="text"] { padding: 10px; border: 1px solid #ccc; 
t                                border-radius: 5px; width: calc(100% - 22px); 
t                                max-width: 300px; margin-right: 10px; }
t   input[type="submit"] { background-color: #3498db; color: white; 
t                          padding: 10px 20px; border: none; border-radius: 5px; 
t                          cursor: pointer; font-weight: bold; }
t   input[type="submit"]:hover { background-color: #2980b9; }
t   .rx-box { background-color: #2c3e50; color: #2ecc71; padding: 15px; 
t             border-radius: 5px; font-family: 'Courier New', monospace; 
t             letter-spacing: 2px; font-size: 1.2em; text-align: center; }
t </style>
t </head>
t <body>
t <div class="container">
t   <div class="header">
t     <img src="logo.png" alt="REACT Logo" class="logo">
t     <h1>Estaci&oacute;n Base R.E.A.C.T.</h1>
t   </div>
t   <form action="panel.cgi" method="post" name="form_react">
t   <input type="hidden" value="react" name="pg">
t   
t   <h2>Estado del Sistema</h2>
t   <div class="info-box">
t     <strong>Estado del Nodo B (Consumo):</strong>
c r 1
t   </div>
t
t   <h2>Control de Entrenamiento Cognitivo</h2>
t   <div class="form-group">
t     <label><strong>Seleccione el modo de operaci&oacute;n:</strong></label><br><br>
t     <select name="modo">
t       <option value="1">Memoria de Trabajo</option>
t       <option value="2">Control de Fuerza</option>
t       <option value="3">Inhibici&oacute;n Motora</option>
t       <option value="4">Ritmo Constante</option>
t       <option value="5">Discriminaci&oacute;n</option>
t     </select>
t     <input type="submit" value="Enviar Comando">
t   </div>
t
t   <h2>Debugger: Enlace Fibra &Oacute;ptica (UART)</h2>
t   <div class="form-group">
t     <label><strong>Trama TX Manual (Hex - 6 Bytes):</strong></label><br><br>
t     <input type="text" name="trama_tx" maxlength="12" placeholder="Ej: 01A2B3C4D5E6">
t     <input type="submit" value="Transmitir a Nodo B">
t   </div>
t   <div class="form-group">
t     <label><strong>&Uacute;ltima Trama Recibida (RX):</strong></label><br><br>
t     <div class="rx-box">
c r 2
t     </div>
t   </div>
t   
t   </form>
t </div>
t </body>
t </html>
.
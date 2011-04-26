#ifndef IRPINTERPRETER_H
#define IRPINTERPRETER_H

void put_fusent_req(FUSENT_REQ *req, PIRP pirp);
void append_fusent_create_req(FUSENT_CREATE_REQ *creq, PIRP pirp);
void append_fusent_write_req(FUSENT_WRITE_REQ *wreq, PIRP pirp);

FUSENT_REQ *get_fusent_req(PIRP pirp);
FUSENT_CREATE_REQ *get_fusent_create_req(PIRP pirp);
FUSENT_WRITE_REQ *get_fusent_write_req(PIRP pirp);

void put_fusent_generic_resp(FUSENT_GENERIC_RESP *gresp, PIRP pirp);
void append_fusent_mount(FUSENT_MOUNT *mount, PIRP pirp);

FUSENT_GENERIC_RESP *get_fusent_generic_resp(PIRP pirp);
FUSENT_MOUNT *get_fusent_mount(PIRP pirp);


#endif IRPINTERPRETER_H
#include "hippodrome/order_queue.h"

void hu_oq_init(hu_order_queue* q) {
    q->open_frame = HU_OQ_OPEN_FRAME;
    q->pick_frame = HU_OQ_PICK_FRAME;
    hu_oq_reset(q);
}

void hu_oq_reset(hu_order_queue* q) {
    q->state = HU_OQ_HIDDEN;
    q->slot = -1;
    q->entering = false;
    q->frame = 0;
}

static bool go(hu_order_queue* q, hu_oq_state s) {
    q->state = (uint8_t)s;
    if (s != HU_OQ_MOVE_QUEUED && s != HU_OQ_SWITCH_QUEUED) {
        q->slot = -1;
    }
    return true;
}

bool hu_oq_z(hu_order_queue* q) {
    if (q->entering) {
        hu_oq_reset(q); /* Z during the command selection takes the order back */
        return true;
    }
    return go(q, q->state == HU_OQ_HIDDEN ? HU_OQ_SHOWING : HU_OQ_HIDDEN);
}

bool hu_oq_a(hu_order_queue* q) {
    return !q->entering && q->state == HU_OQ_SHOWING && go(q, HU_OQ_MOVE_SELECT);
}

bool hu_oq_b(hu_order_queue* q) {
    return !q->entering && q->state == HU_OQ_SHOWING && go(q, HU_OQ_SWITCH_SELECT);
}

bool hu_oq_l(hu_order_queue* q) {
    if (q->entering) {
        return false;
    }
    switch (q->state) {
        case HU_OQ_MOVE_SELECT:
        case HU_OQ_SWITCH_SELECT:
            return go(q, HU_OQ_SHOWING);
        case HU_OQ_MOVE_QUEUED:
            return go(q, HU_OQ_MOVE_SELECT);
        case HU_OQ_SWITCH_QUEUED:
            return go(q, HU_OQ_SWITCH_SELECT);
        default:
            return false;
    }
}

bool hu_oq_pick(hu_order_queue* q, int slot) {
    if (q->entering || slot < 0 || slot > 127) {
        return false;
    }
    if (q->state == HU_OQ_MOVE_SELECT) {
        go(q, HU_OQ_MOVE_QUEUED);
    } else if (q->state == HU_OQ_SWITCH_SELECT) {
        go(q, HU_OQ_SWITCH_QUEUED);
    } else {
        return false;
    }
    q->slot = (int8_t)slot;
    return true;
}

void hu_oq_resolution_end(hu_order_queue* q) {
    if (q->entering) {
        return;
    }
    if (!hu_oq_queued(q)) {
        hu_oq_reset(q);
        return;
    }
    q->entering = true;
    q->frame = 0;
}

hu_oq_action hu_oq_command_frame(hu_order_queue* q) {
    hu_oq_action a = HU_OQ_NONE;
    if (!q->entering) {
        return HU_OQ_NONE;
    }
    q->frame++;
    if (q->frame == q->open_frame) {
        a = q->state == HU_OQ_MOVE_QUEUED ? HU_OQ_OPEN_FIGHT : HU_OQ_OPEN_SWITCH;
    } else if (q->frame >= q->pick_frame) {
        a = HU_OQ_PICK;
        q->entering = false;
        q->state = HU_OQ_HIDDEN; /* slot stays readable for the port until the next change */
    }
    return a;
}

void hu_oq_cancel(hu_order_queue* q) {
    hu_oq_reset(q);
}

bool hu_oq_queued(const hu_order_queue* q) {
    return q->state == HU_OQ_MOVE_QUEUED || q->state == HU_OQ_SWITCH_QUEUED;
}

bool hu_oq_visible(const hu_order_queue* q) {
    return q->state != HU_OQ_HIDDEN && !q->entering;
}
